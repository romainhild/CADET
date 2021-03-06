// =============================================================================
//  CADET - The Chromatography Analysis and Design Toolkit
//  
//  Copyright © 2008-2018: The CADET Authors
//            Please see the AUTHORS and CONTRIBUTORS file.
//  
//  All rights reserved. This program and the accompanying materials
//  are made available under the terms of the GNU Public License v3.0 (or, at
//  your option, any later version) which accompanies this distribution, and
//  is available at http://www.gnu.org/licenses/gpl.html
// =============================================================================

#include "model/BindingModel.hpp"
#include "ParamIdUtil.hpp"

#include <unordered_map>

namespace cadet
{

namespace model
{

/**
 * @brief Defines the dummy binding model
 * @details The dummy binding model does absolutely nothing.
 */
class DummyBinding : public IBindingModel
{
public:

	DummyBinding() { }
	virtual ~DummyBinding() CADET_NOEXCEPT { }

	static const char* identifier() { return "NONE"; }
	virtual const char* name() const CADET_NOEXCEPT { return "NONE"; }
	virtual bool requiresConfiguration() const CADET_NOEXCEPT { return false; }

	virtual void configureModelDiscretization(unsigned int nComp, unsigned int const* nBound, unsigned int const* boundOffset)
	{
	}

	virtual bool configure(IParameterProvider& paramProvider, unsigned int unitOpIdx)
	{
		return true;
	}

	virtual bool reconfigure(IParameterProvider& paramProvider, unsigned int unitOpIdx)
	{
		return configure(paramProvider, unitOpIdx);
	}

	virtual std::unordered_map<ParameterId, double> getAllParameterValues() const
	{
		return std::unordered_map<ParameterId, double>();
	}

	virtual bool hasParameter(const ParameterId& pId) const
	{
		return false;
	}

	virtual bool setParameter(const ParameterId& pId, int value)
	{
		return false;
	}

	virtual bool setParameter(const ParameterId& pId, double value)
	{
		return false;
	}

	virtual bool setParameter(const ParameterId& pId, bool value)
	{
		return false;
	}

	virtual void getAlgebraicBlock(unsigned int& idxStart, unsigned int& len) const
	{
		idxStart = 0;
		len = 0;
	}

	virtual active* getParameter(const ParameterId& pId)
	{
		return nullptr;
	}

	virtual unsigned int workspaceSize() const { return 0; }

	virtual void consistentInitialState(double t, double z, double r, unsigned int secIdx, double* const vecStateY, double errorTol, 
		active* const adRes, active* const adY, unsigned int adEqOffset, unsigned int adDirOffset, const ad::IJacobianExtractor& jacExtractor, 
		double* const workingMemory, linalg::detail::DenseMatrixBase& workingMat) const
	{
	}

	virtual int residual(const active& t, double z, double r, unsigned int secIdx, const active& timeFactor, 
		active const* y, double const* yDot, active* res, void* workSpace) const
	{
		return 0;
	}

	virtual int residual(double t, double z, double r, unsigned int secIdx, double timeFactor, 
		active const* y, double const* yDot, active* res, void* workSpace) const
	{
		return 0;
	}

	virtual int residual(const active& t, double z, double r, unsigned int secIdx, const active& timeFactor, 
		double const* y, double const* yDot, active* res, void* workSpace) const
	{
		return 0;
	}

	virtual int residual(double t, double z, double r, unsigned int secIdx, double timeFactor, 
		double const* y, double const* yDot, double* res, void* workSpace) const
	{
		return 0;
	}

	virtual void setExternalFunctions(IExternalFunction** extFuns, unsigned int size) { }

	virtual void analyticJacobian(double t, double z, double r, unsigned int secIdx, double const* y, linalg::BandMatrix::RowIterator jac, void* workSpace) const
	{
	}

	virtual void analyticJacobian(double t, double z, double r, unsigned int secIdx, double const* y, linalg::DenseBandedRowIterator jac, void* workSpace) const
	{
	}

	virtual void jacobianAddDiscretized(double alpha, linalg::FactorizableBandMatrix::RowIterator jac) const
	{
	}

	virtual void jacobianAddDiscretized(double alpha, linalg::DenseBandedRowIterator jac) const
	{
	}

	virtual void multiplyWithDerivativeJacobian(double const* yDotS, double* const res, double timeFactor) const
	{
	}

	virtual void timeDerivativeAlgebraicResidual(double t, double z, double r, unsigned int secIdx, double const* y, double* dResDt, void* workSpace) const
	{
	}

	virtual bool hasSalt() const CADET_NOEXCEPT { return false; }
	virtual bool supportsMultistate() const CADET_NOEXCEPT { return true; }
	virtual bool supportsNonBinding() const CADET_NOEXCEPT { return true; }
	virtual bool hasAlgebraicEquations() const CADET_NOEXCEPT { return false; }
	virtual bool dependsOnTime() const CADET_NOEXCEPT { return false; }
	virtual bool requiresWorkspace() const CADET_NOEXCEPT { return false; }

protected:
};

namespace binding
{
	void registerDummyModel(std::unordered_map<std::string, std::function<model::IBindingModel*()>>& bindings)
	{
		bindings[DummyBinding::identifier()] = []() { return new DummyBinding(); };
		bindings["DUMMY"] = []() { return new DummyBinding(); };
	}
}  // namespace binding

}  // namespace model

}  // namespace cadet
