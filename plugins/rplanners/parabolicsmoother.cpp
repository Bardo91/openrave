// -*- coding: utf-8 -*-
// Copyright (C) 2012-2013 Rosen Diankov <rosen.diankov@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#include "plugindefs.h"
#include <fstream>

#include <openrave/planningutils.h>

#include "ParabolicPathSmooth/DynamicPath.h"

namespace ParabolicRamp = ParabolicRampInternal;

class ParabolicSmoother : public PlannerBase, public ParabolicRamp::FeasibilityCheckerBase, public ParabolicRamp::RandomNumberGeneratorBase
{
public:
    ParabolicSmoother(EnvironmentBasePtr penv, std::istream& sinput) : PlannerBase(penv)
    {
        __description = ":Interface Author: Rosen Diankov\n\nInterface to `Indiana University Intelligent Motion Laboratory <http://www.iu.edu/~motion/software.html>`_ parabolic smoothing library (Kris Hauser).\n\n**Note:** The original trajectory will not be preserved at all, don't use this if the robot has to hit all points of the trajectory.\n";
    }

    virtual bool InitPlan(RobotBasePtr pbase, PlannerParametersConstPtr params)
    {
        EnvironmentMutex::scoped_lock lock(GetEnv()->GetMutex());
        _parameters.reset(new TrajectoryTimingParameters());
        _parameters->copy(params);
        return _InitPlan();
    }

    virtual bool InitPlan(RobotBasePtr pbase, std::istream& isParameters)
    {
        EnvironmentMutex::scoped_lock lock(GetEnv()->GetMutex());
        _parameters.reset(new TrajectoryTimingParameters());
        isParameters >> *_parameters;
        return _InitPlan();
    }

    bool _InitPlan()
    {
        if( _parameters->_nMaxIterations <= 0 ) {
            _parameters->_nMaxIterations = 100;
        }
        _bUsePerturbation = true;
        if( !_uniformsampler ) {
            _uniformsampler = RaveCreateSpaceSampler(GetEnv(),"mt19937");
        }
        _uniformsampler->SetSeed(_parameters->_randomgeneratorseed);
        //_uniformsampler->SetSeed(utils::GetMilliTime()); // use only for testing
        return !!_uniformsampler;
    }

    virtual PlannerParametersConstPtr GetParameters() const {
        return _parameters;
    }

    virtual PlannerStatus PlanPath(TrajectoryBasePtr ptraj)
    {
        BOOST_ASSERT(!!_parameters && !!ptraj);
        if( ptraj->GetNumWaypoints() < 2 ) {
            return PS_Failed;
        }

        // save velocities
        std::vector<KinBody::KinBodyStateSaverPtr> vstatesavers;
        std::vector<KinBodyPtr> vusedbodies;
        _parameters->_configurationspecification.ExtractUsedBodies(GetEnv(), vusedbodies);
        if( vusedbodies.size() == 0 ) {
            RAVELOG_WARN("there are no used bodies in this configuration\n");
        }

        FOREACH(itbody, vusedbodies) {
            KinBody::KinBodyStateSaverPtr statesaver;
            if( (*itbody)->IsRobot() ) {
                statesaver.reset(new RobotBase::RobotStateSaver(RaveInterfaceCast<RobotBase>(*itbody), KinBody::Save_LinkTransformation|KinBody::Save_LinkEnable|KinBody::Save_ActiveDOF|KinBody::Save_ActiveManipulator|KinBody::Save_LinkVelocities));
            }
            else {
                statesaver.reset(new KinBody::KinBodyStateSaver(*itbody, KinBody::Save_LinkTransformation|KinBody::Save_LinkEnable|KinBody::Save_ActiveDOF|KinBody::Save_ActiveManipulator|KinBody::Save_LinkVelocities));
            }
            vstatesavers.push_back(statesaver);
        }

        uint32_t basetime = utils::GetMilliTime();
        TrajectoryTimingParametersConstPtr parameters = boost::dynamic_pointer_cast<TrajectoryTimingParameters const>(GetParameters());

        vector<ParabolicRamp::Vector> path;
        path.reserve(ptraj->GetNumWaypoints());
        vector<dReal> vtrajpoints;
        ptraj->GetWaypoints(0,ptraj->GetNumWaypoints(),vtrajpoints,_parameters->_configurationspecification);
        ParabolicRamp::Vector q(_parameters->GetDOF());
        for(size_t i = 0; i < ptraj->GetNumWaypoints(); ++i) {
            std::copy(vtrajpoints.begin()+i*_parameters->GetDOF(),vtrajpoints.begin()+(i+1)*_parameters->GetDOF(),q.begin());
            if( path.size() >= 2 ) {
                // check if collinear by taking angle
                const ParabolicRamp::Vector& x0 = path[path.size()-2];
                const ParabolicRamp::Vector& x1 = path[path.size()-1];
                dReal dotproduct=0,x0length2=0,x1length2=0;
                for(size_t i = 0; i < q.size(); ++i) {
                    dReal dx0=x0[i]-q[i];
                    dReal dx1=x1[i]-q[i];
                    dotproduct += dx0*dx1;
                    x0length2 += dx0*dx0;
                    x1length2 += dx1*dx1;
                }
                if( RaveFabs(dotproduct * dotproduct - x0length2*x1length2) < 1e-8 ) {
                    path.back() = q;
                    continue;
                }
            }
            // check if the point is not the same as the previous point
            if( path.size() > 0 ) {
                dReal d = 0;
                for(size_t i = 0; i < q.size(); ++i) {
                    d += RaveFabs(q[i]-path.back().at(i));
                }
                if( d <= q.size()*std::numeric_limits<dReal>::epsilon() ) {
                    continue;
                }
            }
            path.push_back(q);
        }
        try {
            _bUsePerturbation = true;
            ParabolicRamp::DynamicPath dynamicpath;
            dynamicpath.Init(parameters->_vConfigVelocityLimit,parameters->_vConfigAccelerationLimit);
            dynamicpath._multidofinterp = _parameters->_multidofinterp;
            dynamicpath.SetJointLimits(parameters->_vConfigLowerLimit,parameters->_vConfigUpperLimit);
            dynamicpath.SetMilestones(path);   //now the trajectory starts and stops at every milestone
            RAVELOG_DEBUG(str(boost::format("initial path size=%d, duration=%f, pointtolerance=%f, multidof=%d")%path.size()%dynamicpath.GetTotalTime()%parameters->_pointtolerance%_parameters->_multidofinterp));
            ParabolicRamp::Vector tol = parameters->_vConfigResolution;
            FOREACH(it,tol) {
                *it *= parameters->_pointtolerance;
            }
            ParabolicRamp::RampFeasibilityChecker checker(this,tol);

            PlannerProgress progress; progress._iteration=0;
            if( _CallCallbacks(progress) == PA_Interrupt ) {
                return PS_Interrupted;
            }

            int numshortcuts=0;
            if( !!parameters->_setstatefn ) {
                // no idea what a good mintimestep is... _parameters->_fStepLength*0.5?
                numshortcuts = dynamicpath.Shortcut(parameters->_nMaxIterations,checker,this, parameters->_fStepLength*0.99);
            }

            progress._iteration=1;
            if( _CallCallbacks(progress) == PA_Interrupt ) {
                return PS_Interrupted;
            }

            ConfigurationSpecification oldspec = parameters->_configurationspecification;
            ConfigurationSpecification velspec = oldspec.ConvertToVelocitySpecification();
            ConfigurationSpecification newspec = oldspec;
            newspec.AddDerivativeGroups(1,true);
            int waypointoffset = newspec.AddGroup("iswaypoint", 1, "next");

            int timeoffset=-1;
            FOREACH(itgroup,newspec._vgroups) {
                if( itgroup->name == "deltatime" ) {
                    timeoffset = itgroup->offset;
                }
                else if( velspec.FindCompatibleGroup(*itgroup) != velspec._vgroups.end() ) {
                    itgroup->interpolation = "linear";
                }
                else if( oldspec.FindCompatibleGroup(*itgroup) != oldspec._vgroups.end() ) {
                    itgroup->interpolation = "quadratic";
                }
            }

            // have to write to another trajectory
            if( !_dummytraj || _dummytraj->GetXMLId() != ptraj->GetXMLId() ) {
                _dummytraj = RaveCreateTrajectory(GetEnv(), ptraj->GetXMLId());
            }
            _dummytraj->Init(newspec);

            // separate all the acceleration switches into individual points
            vtrajpoints.resize(newspec.GetDOF());
            ConfigurationSpecification::ConvertData(vtrajpoints.begin(),newspec,dynamicpath.ramps.at(0).x0.begin(),oldspec,1,GetEnv(),true);
            ConfigurationSpecification::ConvertData(vtrajpoints.begin(),newspec,dynamicpath.ramps.at(0).dx0.begin(),velspec,1,GetEnv(),false);
            vtrajpoints.at(waypointoffset) = 1;
            vtrajpoints.at(timeoffset) = 0;
            _dummytraj->Insert(_dummytraj->GetNumWaypoints(),vtrajpoints);
            vector<dReal> vswitchtimes;
            ParabolicRamp::Vector vconfig;
            std::vector<ParabolicRamp::ParabolicRampND> temprampsnd;
            FOREACH(itrampnd,dynamicpath.ramps) {
                temprampsnd.resize(1);
                temprampsnd[0] = *itrampnd;
                // double-check the current ramps
                if(!itrampnd->constraintchecked ) {
                    // part of original trajectory which might not have been processed with perterbations, so ignore them
                    _bUsePerturbation = false;
                    if( !checker.Check(*itrampnd)) {
                        std::vector<std::vector<ParabolicRamp::ParabolicRamp1D> > tempramps1d;
                        // try to time scale, perhaps collision and dynamics will change
                        // go all the way up to 2.0 multiplier: 1.05*1.1*1.15*1.2*1.25 ~= 2
                        bool bSuccess = false;
                        dReal mult = 1.05;
                        dReal endTime = itrampnd->endTime;
                        for(size_t idilate = 0; idilate < 5; ++idilate ) {
                            tempramps1d.resize(0);
                            endTime *= mult;
                            if( ParabolicRamp::SolveAccelBounded(itrampnd->x0, itrampnd->dx0, itrampnd->x1, itrampnd->dx1, endTime,  parameters->_vConfigAccelerationLimit, parameters->_vConfigVelocityLimit, parameters->_vConfigLowerLimit, parameters->_vConfigUpperLimit, tempramps1d, _parameters->_multidofinterp) ) {
                                temprampsnd.resize(0);
                                CombineRamps(tempramps1d, temprampsnd);
                                bool bHasBadRamp=false;
                                FOREACH(itnewrampnd, temprampsnd) {
                                    if( !checker.Check(*itnewrampnd) ) {
                                        bHasBadRamp = true;
                                        break;
                                    }
                                }
                                if( !bHasBadRamp ) {
                                    bSuccess = true;
                                    temprampsnd = temprampsnd;
                                    break;
                                }
                                mult += 0.05;
                            }
                            if( !bSuccess ) {
                                throw OPENRAVE_EXCEPTION_FORMAT0("original ramp is in collision!", ORE_Assert);
                            }
                        }
                    }
                    _bUsePerturbation = true; // re-enable
                    progress._iteration=2;
                    if( _CallCallbacks(progress) == PA_Interrupt ) {
                        return PS_Interrupted;
                    }
                }

                FOREACH(itrampnd2, temprampsnd) {
                    vswitchtimes.resize(0);
                    vswitchtimes.push_back(itrampnd->endTime);
                    if( _parameters->_outputaccelchanges ) {
                        FOREACHC(itramp,itrampnd->ramps) {
                            vector<dReal>::iterator it;
                            if( itramp->tswitch1 != 0 ) {
                                it = lower_bound(vswitchtimes.begin(),vswitchtimes.end(),itramp->tswitch1);
                                if( *it != itramp->tswitch1) {
                                    vswitchtimes.insert(it,itramp->tswitch1);
                                }
                            }
                            if( itramp->tswitch1 != itramp->tswitch2 && itramp->tswitch2 != 0 ) {
                                it = lower_bound(vswitchtimes.begin(),vswitchtimes.end(),itramp->tswitch2);
                                if( *it != itramp->tswitch2 ) {
                                    vswitchtimes.insert(it,itramp->tswitch2);
                                }
                            }
                        }
                    }
                    vtrajpoints.resize(newspec.GetDOF()*vswitchtimes.size());
                    vector<dReal>::iterator ittargetdata = vtrajpoints.begin();
                    dReal prevtime = 0;
                    for(size_t i = 0; i < vswitchtimes.size(); ++i) {
                        itrampnd->Evaluate(vswitchtimes[i],vconfig);
                        ConfigurationSpecification::ConvertData(ittargetdata,newspec,vconfig.begin(),oldspec,1,GetEnv(),true);
                        itrampnd->Derivative(vswitchtimes[i],vconfig);
                        ConfigurationSpecification::ConvertData(ittargetdata,newspec,vconfig.begin(),velspec,1,GetEnv(),false);
                        *(ittargetdata+timeoffset) = vswitchtimes[i]-prevtime;
                        *(ittargetdata+waypointoffset) = dReal(i+1==vswitchtimes.size());
                        ittargetdata += newspec.GetDOF();
                        prevtime = vswitchtimes[i];
                    }
                    _dummytraj->Insert(_dummytraj->GetNumWaypoints(),vtrajpoints);
                }
            }

            BOOST_ASSERT(RaveFabs(dynamicpath.GetTotalTime()-_dummytraj->GetDuration())<0.001);
            RAVELOG_DEBUG(str(boost::format("after shortcutting %d times: path waypoints=%d, traj waypoints=%d, traj time=%fs")%numshortcuts%dynamicpath.ramps.size()%_dummytraj->GetNumWaypoints()%dynamicpath.GetTotalTime()));
            ptraj->Swap(_dummytraj);
        }
        catch (const std::exception& ex) {
            if( IS_DEBUGLEVEL(Level_Verbose) ) {
                string filename = str(boost::format("%s/failedsmoothing%d.xml")%RaveGetHomeDirectory()%(RaveRandomInt()%10000));
                RAVELOG_WARN(str(boost::format("parabolic planner failed: %s, writing original trajectory to %s")%ex.what()%filename));
                ofstream f(filename.c_str());
                f << std::setprecision(std::numeric_limits<dReal>::digits10+1);
                ptraj->serialize(f);
            }
            else {
                RAVELOG_WARN(str(boost::format("parabolic planner failed: %s")%ex.what()));
            }
            return PS_Failed;
        }
        RAVELOG_DEBUG(str(boost::format("path optimizing - computation time=%fs\n")%(0.001f*(float)(utils::GetMilliTime()-basetime))));
        return _ProcessPostPlanners(RobotBasePtr(),ptraj);
    }

    virtual bool ConfigFeasible(const ParabolicRamp::Vector& a, const ParabolicRamp::Vector& da)
    {
        if( _bUsePerturbation ) {
            // have to also test with tolerances!
            boost::array<dReal,3> perturbations = {{ 0,_parameters->_pointtolerance,-_parameters->_pointtolerance}};
            ParabolicRamp::Vector anew(a.size());
            FOREACH(itperturbation,perturbations) {
                for(size_t i = 0; i < a.size(); ++i) {
                    anew[i] = a[i] + *itperturbation * _parameters->_vConfigResolution.at(i);
                }
                //_parameters->_setstatefn(anew);
                if( _parameters->CheckPathAllConstraints(anew,anew,da, da, 0, IT_OpenStart) <= 0 ) {
                    return false;
                }
            }
        }
        else {
            //_parameters->_setstatefn(a);
            if( _parameters->CheckPathAllConstraints(a,a, da, da, 0, IT_OpenStart) <= 0 ) {
                return false;
            }
        }
        return true;
    }

    virtual bool SegmentFeasible(const ParabolicRamp::Vector& a,const ParabolicRamp::Vector& b, const ParabolicRamp::Vector& da,const ParabolicRamp::Vector& db, dReal timeelapsed)
    {
        if( _bUsePerturbation ) {
            // test with tolerances!
            boost::array<dReal,3> perturbations = {{ 0,_parameters->_pointtolerance,-_parameters->_pointtolerance }};
            ParabolicRamp::Vector anew(a.size()), bnew(b.size());
            FOREACH(itperturbation,perturbations) {
                for(size_t i = 0; i < a.size(); ++i) {
                    anew[i] = a[i] + *itperturbation * _parameters->_vConfigResolution.at(i);
                    bnew[i] = b[i] + *itperturbation * _parameters->_vConfigResolution.at(i);
                }
                //_parameters->_setstatefn(anew);
                if( _parameters->CheckPathAllConstraints(anew,bnew,da, db, timeelapsed, IT_OpenStart) <= 0 ) {
                    return false;
                }
            }
        }
        else {
            //_parameters->_setstatefn(a);
            if( _parameters->CheckPathAllConstraints(a,b,da, db, timeelapsed, IT_OpenStart) <= 0 ) {
                return false;
            }
        }
        return true;
    }

    virtual ParabolicRamp::Real Rand()
    {
        return _uniformsampler->SampleSequenceOneReal(IT_OpenEnd);
    }

protected:
    TrajectoryTimingParametersPtr _parameters;
    SpaceSamplerBasePtr _uniformsampler;
    bool _bUsePerturbation;
    TrajectoryBasePtr _dummytraj;
};


PlannerBasePtr CreateParabolicSmoother(EnvironmentBasePtr penv, std::istream& sinput)
{
    return PlannerBasePtr(new ParabolicSmoother(penv,sinput));
}

#ifdef RAVE_REGISTER_BOOST
#include BOOST_TYPEOF_INCREMENT_REGISTRATION_GROUP()
BOOST_TYPEOF_REGISTER_TYPE(ParabolicRamp::ParabolicRamp1D)
BOOST_TYPEOF_REGISTER_TYPE(ParabolicRamp::ParabolicRampND)
#endif
