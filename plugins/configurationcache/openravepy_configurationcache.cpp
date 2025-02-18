// -*- Coding: utf-8 -*-
// Copyright (C) 2014 Rosen Diankov
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/// \author Rosen Diankov
#include "configurationcachetree.h"

#define PY_ARRAY_UNIQUE_SYMBOL PyArrayHandle
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#include <boost/python/exception_translator.hpp>
#include <boost/python/docstring_options.hpp>
#include <pyconfig.h>
#include <openrave/xmlreaders.h>

using namespace OpenRAVE;
using namespace boost::python;

// declared from openravepy_int
namespace openravepy {

Transform ExtractTransform(const object& oraw);
TransformMatrix ExtractTransformMatrix(const object& oraw);
object toPyArray(const TransformMatrix& t);
object toPyArray(const Transform& t);

XMLReadablePtr ExtractXMLReadable(object o);
object toPyXMLReadable(XMLReadablePtr p);
bool ExtractIkParameterization(object o, IkParameterization& ikparam);
object toPyIkParameterization(const IkParameterization& ikparam);
object toPyIkParameterization(const std::string& serializeddata);

object toPyPlannerParameters(PlannerBase::PlannerParametersPtr params);

object toPyEnvironment(object);
object toPyKinBody(KinBodyPtr, object opyenv);
object toPyKinBodyLink(KinBody::LinkPtr plink, object opyenv);

//EnvironmentBasePtr GetEnvironment(boost::python::object);
TrajectoryBasePtr GetTrajectory(object);
KinBodyPtr GetKinBody(object);
KinBody::LinkPtr GetKinBodyLink(object);
RobotBasePtr GetRobot(object o);
RobotBase::ManipulatorPtr GetRobotManipulator(object);
PlannerBase::PlannerParametersConstPtr GetPlannerParametersConst(object);
CollisionReportPtr GetCollisionReport(object);

EnvironmentBasePtr GetEnvironment(object o);
}

namespace configurationcachepy {

inline std::string _ExtractStringSafe(boost::python::object ostring)
{
    if( ostring == object() ) {
        return std::string();
    }
    else {
        return extract<std::string>(ostring);
    }
}

inline boost::python::object ConvertStringToUnicode(const std::string& s)
{
    return boost::python::object(boost::python::handle<>(PyUnicode_Decode(s.c_str(),s.size(), "utf-8", NULL)));
}

template <typename T>
inline RaveVector<T> ExtractVector3Type(const object& o)
{
    return RaveVector<T>(extract<T>(o[0]), extract<T>(o[1]), extract<T>(o[2]));
}

template <typename T>
inline std::vector<T> ExtractArray(const object& o)
{
    std::vector<T> v(len(o));
    for(size_t i = 0; i < v.size(); ++i) {
        v[i] = extract<T>(o[i]);
    }
    return v;
}

inline numpy::ndarray toPyArrayN(const float* pvalues, size_t N)
{
    if( N == 0 ) {
        return numpy::array(boost::python::list());
    }
    numpy::dtype dt = numpy::dtype::get_builtin<float>();
    boost::python::tuple shape = boost::python::make_tuple(N);
    numpy::ndarray pyarray = numpy::empty(shape, dt);
    std::memcpy(pyarray.get_data(), &pvalues[0], N * sizeof(float));
    return pyarray;
}

inline numpy::ndarray toPyArrayN(const float* pvalues, const std::vector<size_t>& shape)
{
    if( shape.empty() ) {
        return numpy::array(boost::python::list());
    }
    size_t size = sizeof(float);
    for (auto && dim : shape) {
      size *= dim;
    }
    numpy::dtype dt = numpy::dtype::get_builtin<float>();
    numpy::ndarray pyarray = numpy::empty(boost::python::tuple(shape), dt);
    std::memcpy(pyarray.get_data(), &pvalues[0], size);
    return pyarray;
}

inline numpy::ndarray toPyArrayN(const double* pvalues, size_t N)
{
    if( N == 0 ) {
        return numpy::array(boost::python::list());
    }
    numpy::dtype dt = numpy::dtype::get_builtin<double>();
    boost::python::tuple shape = boost::python::tuple(N);
    numpy::ndarray pyarray = numpy::empty(shape, dt);
    std::memcpy(pyarray.get_data(), &pvalues[0], N * sizeof(double));
    return pyarray;
}

inline numpy::ndarray toPyArrayN(const double* pvalues, const std::vector<size_t>& shape)
{
    if( shape.empty() ) {
        return numpy::array(boost::python::list());
    }
    size_t size = sizeof(double);
    for (auto && dim : shape) {
      size *= dim;
    }
    numpy::dtype dt = numpy::dtype::get_builtin<double>();
    numpy::ndarray pyarray = numpy::empty(boost::python::tuple(shape), dt);
    std::memcpy(pyarray.get_data(), &pvalues[0], size);
    return pyarray;
}

inline numpy::ndarray toPyArrayN(const uint8_t* pvalues, const std::vector<size_t>& shape)
{
    if( shape.empty() ) {
        return numpy::array(boost::python::list());
    }
    size_t size = sizeof(uint8_t);
    for (auto && dim : shape) {
      size *= dim;
    }
    numpy::dtype dt = numpy::dtype::get_builtin<uint8_t>();
    numpy::ndarray pyarray = numpy::empty(boost::python::tuple(shape), dt);
    std::memcpy(pyarray.get_data(), &pvalues[0], size);
    return pyarray;
}

inline numpy::ndarray toPyArrayN(const uint8_t* pvalues, size_t N)
{
    if( N == 0 ) {
        return numpy::array(boost::python::list());
    }
    numpy::dtype dt = numpy::dtype::get_builtin<uint8_t>();
    boost::python::tuple shape = boost::python::tuple(N);
    numpy::ndarray pyarray = numpy::empty(shape, dt);
    std::memcpy(pyarray.get_data(), &pvalues[0], N * sizeof(uint8_t));
    return pyarray;
}

inline numpy::ndarray toPyArrayN(const int* pvalues, size_t N)
{
    if( N == 0 ) {
        return numpy::array(boost::python::list());
    }
    numpy::dtype dt = numpy::dtype::get_builtin<int>();
    boost::python::tuple shape = boost::python::tuple(N);
    numpy::ndarray pyarray = numpy::empty(shape, dt);
    std::memcpy(pyarray.get_data(), &pvalues[0], N * sizeof(int));
    return pyarray;
}

inline numpy::ndarray toPyArrayN(const uint32_t* pvalues, size_t N)
{
    if( N == 0 ) {
        return numpy::array(boost::python::list());
    }
    numpy::dtype dt = numpy::dtype::get_builtin<uint32_t>();
    boost::python::tuple shape = boost::python::tuple(N);
    numpy::ndarray pyarray = numpy::empty(shape, dt);
    std::memcpy(pyarray.get_data(), &pvalues[0], N * sizeof(uint32_t));
    return pyarray;
}

template <typename T>
inline object toPyList(const std::vector<T>& v)
{
    boost::python::list lvalues;
    FOREACHC(it,v) {
        lvalues.append(object(*it));
    }
    return std::move(lvalues);
}

template <typename T>
inline numpy::ndarray toPyArray(const std::vector<T>& v)
{
    if( v.size() == 0 ) {
        return toPyArrayN((T*)NULL,0);
    }
    return toPyArrayN(&v[0],v.size());
}

template <typename T>
inline numpy::ndarray toPyArray(const std::vector<T>& v, std::vector<size_t>& dims)
{
    boost::python::tuple dims_tuple(dims);
    if( v.size() == 0 ) {
        return toPyArrayN((T*)NULL,dims_tuple);
    }
    size_t totalsize = 1;
    FOREACH(it,dims)
    totalsize *= *it;
    BOOST_ASSERT(totalsize == v.size());
    return toPyArrayN(&v[0],dims_tuple);
}

template <typename T, int N>
inline numpy::ndarray toPyArray(const boost::array<T,N>& v)
{
    if( v.size() == 0 ) {
        return toPyArrayN((T*)NULL,0);
    }
    return toPyArrayN(&v[0],v.size());
}

class PyConfigurationCache
{
public:
    PyConfigurationCache(object pyrobot)
    {
        _pyenv = openravepy::toPyEnvironment(pyrobot);
        _cache.reset(new configurationcache::ConfigurationCache(openravepy::GetRobot(pyrobot)));
    }
    virtual ~PyConfigurationCache(){
    }

    int InsertConfigurationDist(object ovalues, object pyreport, dReal dist)
    {
        return _cache->InsertConfiguration(ExtractArray<dReal>(ovalues), openravepy::GetCollisionReport(pyreport), dist);
    }

    int InsertConfiguration(object ovalues, object pyreport)
    {
        return _cache->InsertConfiguration(ExtractArray<dReal>(ovalues), openravepy::GetCollisionReport(pyreport));
    }

    object CheckCollision(object ovalues)
    {
        KinBody::LinkConstPtr crobotlink, ccollidinglink;
        dReal closestdist=0;
        int ret = _cache->CheckCollision(ExtractArray<dReal>(ovalues), crobotlink, ccollidinglink, closestdist);
        KinBody::LinkPtr robotlink, collidinglink;
        if( !!crobotlink ) {
            robotlink = crobotlink->GetParent()->GetLinks().at(crobotlink->GetIndex());
        }
        if( !!ccollidinglink ) {
            collidinglink = ccollidinglink->GetParent()->GetLinks().at(ccollidinglink->GetIndex());
        }
        return boost::python::make_tuple(ret, closestdist, boost::python::make_tuple(openravepy::toPyKinBodyLink(robotlink, _pyenv), openravepy::toPyKinBodyLink(collidinglink, _pyenv)));
    }

    void Reset()
    {
        _cache->Reset();
    }

    object GetDOFValues()
    {
        std::vector<dReal> values;
        _cache->GetDOFValues(values);
        return toPyArray(values);
    }

    int GetNumNodes() {
        return _cache->GetNumNodes();
    }

    void SetCollisionThresh(dReal colthresh)
    {
        _cache->SetCollisionThresh(colthresh);
    }

    void SetFreeSpaceThresh(dReal freespacethresh)
    {
        _cache->SetFreeSpaceThresh(freespacethresh);
    }

    void SetWeights(object oweights)
    {
        _cache->SetWeights(ExtractArray<dReal>(oweights));
    }

    void SetInsertionDistanceMult(dReal indist)
    {
        _cache->SetInsertionDistanceMult(indist);
    }

    dReal GetCollisionThresh()
    {
        return _cache->GetCollisionThresh();
    }

    dReal GetFreeSpaceThresh()
    {
        return _cache->GetFreeSpaceThresh();
    }


    dReal GetInsertionDistanceMult()
    {
        return _cache->GetInsertionDistanceMult();
    }

    object GetRobot() {
        return openravepy::toPyKinBody(_cache->GetRobot(), _pyenv);
    }

    bool Validate() {
        return _cache->Validate();
    }

    object GetNodeValues() {
        std::vector<dReal> values;
        _cache->GetNodeValues(values);
        return toPyArray(values);
    }

    object FindNearestNode(object ovalues, dReal dist) {
        std::pair<std::vector<dReal>, dReal> nn = _cache->FindNearestNode(ExtractArray<dReal>(ovalues), dist);
        if( nn.first.size() == 0 ) {
            return object(); // didn't find anything
        }
        else {
           return boost::python::make_tuple(toPyArray(nn.first), nn.second);
        }
    }
    
    dReal ComputeDistance(object oconfi, object oconff) {
        return _cache->ComputeDistance(ExtractArray<dReal>(oconfi), ExtractArray<dReal>(oconff));
    }

protected:
    object _pyenv;
    configurationcache::ConfigurationCachePtr _cache;
};

typedef boost::shared_ptr<PyConfigurationCache> PyConfigurationCachePtr;

} // end namespace configurationcachepy

BOOST_PYTHON_MODULE(openravepy_configurationcache)
{
    using namespace configurationcachepy;
    Py_Initialize();
    numpy::initialize();
    scope().attr("__doc__") = "The module contains configuration cache bindings for openravepy\n";

    class_<PyConfigurationCache, PyConfigurationCachePtr >("ConfigurationCache", no_init)
    .def(init<object>(args("robot")))
    .def("InsertConfigurationDist",&PyConfigurationCache::InsertConfigurationDist, args("values","report","dist"))
    .def("InsertConfiguration",&PyConfigurationCache::InsertConfiguration, args("values", "report"))
    .def("CheckCollision",&PyConfigurationCache::CheckCollision, args("values"))
    .def("Reset",&PyConfigurationCache::Reset)
    .def("GetDOFValues",&PyConfigurationCache::GetDOFValues)
    .def("GetNumNodes",&PyConfigurationCache::GetNumNodes)
    .def("SetCollisionThresh",&PyConfigurationCache::SetCollisionThresh, args("colthresh"))
    .def("SetFreeSpaceThresh",&PyConfigurationCache::SetFreeSpaceThresh, args("freespacethresh"))
    .def("SetWeights",&PyConfigurationCache::SetWeights, args("weights"))
    .def("SetInsertionDistanceMult",&PyConfigurationCache::SetInsertionDistanceMult, args("indist"))
    .def("GetRobot",&PyConfigurationCache::GetRobot)
    .def("GetNumNodes",&PyConfigurationCache::GetNumNodes)
    .def("Validate", &PyConfigurationCache::Validate)
    .def("GetNodeValues", &PyConfigurationCache::GetNodeValues)
    .def("FindNearestNode", &PyConfigurationCache::FindNearestNode)
    .def("ComputeDistance", &PyConfigurationCache::ComputeDistance)

    .def("GetCollisionThresh", &PyConfigurationCache::GetCollisionThresh)
    .def("GetFreeSpaceThresh", &PyConfigurationCache::GetFreeSpaceThresh)
    .def("GetInsertionDistanceMult", &PyConfigurationCache::GetInsertionDistanceMult)
    ;
}
