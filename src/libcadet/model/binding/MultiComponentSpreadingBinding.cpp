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

#include "model/binding/BindingModelBase.hpp"
#include "model/ExternalFunctionSupport.hpp"
#include "model/ModelUtils.hpp"
#include "cadet/Exceptions.hpp"
#include "model/Parameters.hpp"
#include "SlicedVector.hpp"
#include "LocalVector.hpp"

#include <vector>
#include <unordered_map>
#include <functional>

namespace cadet
{

namespace model
{

/*<codegen>
{
	"name": "SpreadingParamHandler",
	"externalName": "ExtSpreadingParamHandler",
	"parameters":
		[
			{ "type": "ComponentBoundStateMajorDependentParameter", "varName": "kA", "confName": "MCSPR_KA"},
			{ "type": "ComponentBoundStateMajorDependentParameter", "varName": "kD", "confName": "MCSPR_KD"},
			{ "type": "ComponentBoundStateMajorDependentParameter", "varName": "qMax", "confName": "MCSPR_QMAX"},
			{ "type": "ScalarComponentDependentParameter", "varName": "k12", "confName": "MCSPR_K12"},
			{ "type": "ScalarComponentDependentParameter", "varName": "k21", "confName": "MCSPR_K21"}
		]
}
</codegen>*/

/* Parameter description
 ------------------------
 kA = Adsorption rate in state-major ordering
 kD = Desorption rate in state-major ordering
 qMax = Capacity in state-major ordering
 k12 = Transition rate from state @f$ q_i^A @f$ to state @f$ q_i^B @f$
 k21 = Transition rate from state @f$ q_i^B @f$ to state @f$ q_i^A @f$
*/

inline const char* SpreadingParamHandler::identifier() CADET_NOEXCEPT { return "MULTI_COMPONENT_SPREADING"; }

inline bool SpreadingParamHandler::validateConfig(unsigned int nComp, unsigned int const* nBoundStates)
{
	if ((_kA.size() != _kD.size()) || (_kA.size() != _qMax.size()) || (_kA.size() < nComp * 2))
		throw InvalidParameterException("MCSPR_KA, MCSPR_KD, and MCSPR_QMAX have to have the same size");
	if ((_k12.size() != _k21.size()) || (_k12.size() < nComp))
		throw InvalidParameterException("MCSPR_K12 and MCSPR_K21 have to have the same size (number of components)");

	return true;
}

inline const char* ExtSpreadingParamHandler::identifier() CADET_NOEXCEPT { return "EXT_MULTI_COMPONENT_SPREADING"; }

inline bool ExtSpreadingParamHandler::validateConfig(unsigned int nComp, unsigned int const* nBoundStates)
{
	if ((_kA.size() != _kD.size()) || (_kA.size() != _qMax.size()) || (_kA.size() < nComp * 2))
		throw InvalidParameterException("EXT_MCSPR_KA, EXT_MCSPR_KD, and EXT_MCSPR_QMAX have to have the same size");
	if ((_k12.size() != _k21.size()) || (_k12.size() < nComp))
		throw InvalidParameterException("EXT_MCSPR_K12 and EXT_MCSPR_K21 have to have the same size (number of components)");

	return true;
}


/**
 * @brief Defines the multi component spreading binding model
 * @details Implements the multi component spreading adsorption model: \f[ \begin{align} 
 *                \frac{\mathrm{d} q_i^A}{\mathrm{d} t} &= \left( k_a^A\: c_{p,i} - k_{AB} q_i^A \right) q_{\text{max},i}^A \left( 1 - \sum_{j=1}^{N_{\text{comp}}} \frac{q_j^A}{q_{\text{max},j}^A} - \sum_{j=1}^{N_{\text{comp}}} \frac{q_j^B}{q_{\text{max},j}^B} \right) - k_d^A q_i^A + k_{BA} q_i^B \\
 *                \frac{\mathrm{d} q_i^B}{\mathrm{d} t} &= \left( k_a^B\: c_{p,i} + k_{AB} q_i^A \right) q_{\text{max},i}^A \left( 1 - \sum_{j=1}^{N_{\text{comp}}} \frac{q_j^A}{q_{\text{max},j}^A} - \sum_{j=1}^{N_{\text{comp}}} \frac{q_j^B}{q_{\text{max},j}^B} \right) - \left( k_d^B + k_{BA} \right) q_i^B
 *          \end{align} \f]
 *          Here, a second bound state $q_i^B$ is added to the Langmuir model and the exchange between the two bound 
 *          states $q_i^A$ and $q_i^B$ is allowed. The second bound state may correspond to a different orientation 
 *          on the surface or a different folding state of the molecule.
 *          While components without bound state (i.e., non-binding components) are supported, all other components must have
 *          @c 2 bound states.
 *          
 *          Internal state vector layout is state-major. First, all components of state A are placed, then all components of state B.
 * @tparam ParamHandler_t Type that can add support for external function dependence
 */
template <class ParamHandler_t>
class MultiComponentSpreadingBindingBase : public PureBindingModelBase
{
public:

	MultiComponentSpreadingBindingBase() { }
	virtual ~MultiComponentSpreadingBindingBase() CADET_NOEXCEPT { }

	static const char* identifier() { return ParamHandler_t::identifier(); }
	virtual const char* name() const CADET_NOEXCEPT { return ParamHandler_t::identifier(); }

	virtual void configureModelDiscretization(unsigned int nComp, unsigned int const* nBound, unsigned int const* boundOffset)
	{
		BindingModelBase::configureModelDiscretization(nComp, nBound, boundOffset);

		const unsigned int numSlices = 2;
		for (unsigned int i = 0; i < nComp; ++i)
		{
			if (nBound[i] == 0)
				continue;

			if (nBound[i] != numSlices)
				throw InvalidParameterException("Multi component spreading binding model requires exactly two bound states for all (binding) components");
		}

		_numBindingComp = numBindingComponents(_nBoundStates, _nComp);

		// Allocate space for parameters
		_paramHandler.reserve(nComp * numSlices, numSlices, nComp, nBound);
	}

	virtual void setExternalFunctions(IExternalFunction** extFuns, unsigned int size) { _paramHandler.setExternalFunctions(extFuns, size); }
	virtual bool dependsOnTime() const CADET_NOEXCEPT { return ParamHandler_t::dependsOnTime(); }

	virtual bool hasSalt() const CADET_NOEXCEPT { return false; }
	virtual bool supportsMultistate() const CADET_NOEXCEPT { return true; }
	virtual bool supportsNonBinding() const CADET_NOEXCEPT { return true; }
	virtual bool requiresWorkspace() const CADET_NOEXCEPT { return hasAlgebraicEquations() || ParamHandler_t::requiresWorkspace(); }

	CADET_PUREBINDINGMODELBASE_BOILERPLATE

protected:
	ParamHandler_t _paramHandler; //!< Handles parameters and their dependence on external functions
	unsigned int _numBindingComp; //!< Number of binding components

	virtual unsigned int paramCacheSize() const CADET_NOEXCEPT { return _paramHandler.cacheSize(); }

	virtual bool configureImpl(bool reconfigure, IParameterProvider& paramProvider, unsigned int unitOpIdx)
	{
		// Read parameters
		_paramHandler.configure(paramProvider, _nComp, _nBoundStates);

		// Register parameters
		_paramHandler.registerParameters(_parameters, unitOpIdx, _nComp, _nBoundStates);

		return true;
	}

	template <typename StateType, typename CpStateType, typename ResidualType, typename ParamType>
	int residualImpl(const ParamType& t, double z, double r, unsigned int secIdx, const ParamType& timeFactor,
		StateType const* y, CpStateType const* yCp, double const* yDot, ResidualType* res, void* workSpace) const
	{
		const typename ParamHandler_t::params_t& p = _paramHandler.update(static_cast<double>(t), z, r, secIdx, _nComp, _nBoundStates, workSpace);

		ResidualType qSum = 1.0;
		unsigned int bndIdx = 0;
		for (int j = 0; j < 2; ++j)
		{
			active const* const localQmax = p.qMax[j];
			for (int i = 0; i < _nComp; ++i)
			{
				// Skip components without bound states (bound state index bndIdx is not advanced)
				if (_nBoundStates[i] == 0)
					continue;

				qSum -= y[bndIdx] / static_cast<ParamType>(localQmax[i]);

				// Next bound component
				++bndIdx;
			}
		}

		// First bound state (A)
		active const* localKa = p.kA[0];
		active const* localKd = p.kD[0];
		active const* localQmax = p.qMax[0];

		bndIdx = 0;
		for (int i = 0; i < _nComp; ++i)
		{
			// Skip components without bound states (bound state index bndIdx is not advanced)
			if (_nBoundStates[i] == 0)
				continue;

			// Residual
			res[bndIdx] = static_cast<ParamType>(localKd[i]) * y[bndIdx] - static_cast<ParamType>(p.k21[i]) * y[bndIdx + _numBindingComp] 
					- (static_cast<ParamType>(localKa[i]) * yCp[i] - static_cast<ParamType>(p.k12[i]) * y[bndIdx]) * static_cast<ParamType>(localQmax[i]) * qSum;

			// Add time derivative if necessary
			if (_kineticBinding && yDot)
			{
				res[bndIdx] += timeFactor * yDot[bndIdx];
			}

			// Next bound component
			++bndIdx;
		}

		// Second bound state (B)
		localKa = p.kA[1];
		localKd = p.kD[1];
		// Still use q_{max}^A here

		for (int i = 0; i < _nComp; ++i)
		{
			// Skip components without bound states (bound state index bndIdx is not advanced)
			if (_nBoundStates[i] == 0)
				continue;

			// Residual
			res[bndIdx] = (static_cast<ParamType>(localKd[i]) + static_cast<ParamType>(p.k21[i])) * y[bndIdx] 
					- (static_cast<ParamType>(localKa[i]) * yCp[i] + static_cast<ParamType>(p.k12[i]) * y[bndIdx - _numBindingComp]) * static_cast<ParamType>(localQmax[i]) * qSum;

			// Add time derivative if necessary
			if (_kineticBinding && yDot)
			{
				res[bndIdx] += timeFactor * yDot[bndIdx];
			}

			// Next bound component
			++bndIdx;
		}

		return 0;
	}

	template <typename RowIterator>
	void jacobianImpl(double t, double z, double r, unsigned int secIdx, double const* y, double const* yCp, RowIterator jac, void* workSpace) const
	{
		const typename ParamHandler_t::params_t& p = _paramHandler.update(t, z, r, secIdx, _nComp, _nBoundStates, workSpace);

		double qSum = 1.0;
		int bndIdx = 0;
		for (int j = 0; j < 2; ++j)
		{
			active const* const localQmax = p.qMax[j];
			for (int i = 0; i < _nComp; ++i)
			{
				// Skip components without bound states (bound state index bndIdx is not advanced)
				if (_nBoundStates[i] == 0)
					continue;

				qSum -= y[bndIdx] / static_cast<double>(localQmax[i]);

				// Next bound component
				++bndIdx;
			}
		}

		active const* localKa = p.kA[0];
		active const* localKd = p.kD[0];
		active const* const qMax1 = p.qMax[0];
		active const* const qMax2 = p.qMax[1];

		// First state (A)
		bndIdx = 0;
		for (int i = 0; i < _nComp; ++i)
		{
			// Skip components without bound states (bound state index bndIdx is not advanced)
			if (_nBoundStates[i] == 0)
				continue;

			const double ka = static_cast<double>(localKa[i]);
			const double kd = static_cast<double>(localKd[i]);

			// dres_i / dc_{p,i}
			jac[i - bndIdx - _nComp] = -ka * static_cast<double>(qMax1[i]) * qSum;
			// Getting to c_{p,i}: -bndIdx takes us to q_0, another -nComp to c_{p,0} and a +i to c_{p,i}.
			//                     This means jac[i - bndIdx - nComp] corresponds to c_{p,i}.

			// Fill dres_i / dq_j^A
			const double factor = (ka * yCp[i] - y[bndIdx] * static_cast<double>(p.k12[i])) * static_cast<double>(qMax1[i]);
			int bndIdx2 = 0;
			for (int j = 0; j < _nComp; ++j)
			{
				// Skip components without bound states (bound state index bndIdx is not advanced)
				if (_nBoundStates[j] == 0)
					continue;

				// dres_i / dq_j^A
				jac[bndIdx2 - bndIdx] = factor / static_cast<double>(qMax1[j]);
				// Getting to q_j: -bndIdx takes us to q_0, another +bndIdx2 to q_j. This means jac[bndIdx2 - bndIdx] corresponds to q_j.

				++bndIdx2;
			}

			// Add to dres_i / dq_i^A
			jac[0] += kd + static_cast<double>(p.k12[i]) * static_cast<double>(qMax1[i]) * qSum; // last summand by product rule

			// Fill dres_i / dq_j^B
			for (int j = 0; j < _nComp; ++j)
			{
				// Skip components without bound states (bound state index bndIdx is not advanced)
				if (_nBoundStates[j] == 0)
					continue;

				// dres_i / dq_j^B
				jac[bndIdx2 - bndIdx] = factor / static_cast<double>(qMax2[j]);
				// Getting to q_j: -bndIdx takes us to q_0, another +bndIdx2 to q_j. This means jac[bndIdx2 - bndIdx] corresponds to q_j.

				++bndIdx2;
			}

			// Add to dres_i / dq_i^B
			jac[_numBindingComp] -= static_cast<double>(p.k21[i]);

			// Advance to next equation and Jacobian row
			++bndIdx;
			++jac;
		}

		// Second state (B)
		localKa = p.kA[1];
		localKd = p.kD[1];

		for (int i = 0; i < _nComp; ++i)
		{
			// Skip components without bound states (bound state index bndIdx is not advanced)
			if (_nBoundStates[i] == 0)
				continue;

			const double ka = static_cast<double>(localKa[i]);
			const double kd = static_cast<double>(localKd[i]);

			// dres_i / dc_{p,i}
			jac[i - bndIdx - _nComp] = -ka * static_cast<double>(qMax1[i]) * qSum;
			// Getting to c_{p,i}: -bndIdx takes us to q_0, another -nComp to c_{p,0} and a +i to c_{p,i}.
			//                     This means jac[i - bndIdx - nComp] corresponds to c_{p,i}.

			// Fill dres_i / dq_j^A
			const double factor = (ka * yCp[i] + y[bndIdx - static_cast<int>(_numBindingComp)] * static_cast<double>(p.k12[i])) * static_cast<double>(qMax1[i]);
			int bndIdx2 = 0;
			for (int j = 0; j < _nComp; ++j)
			{
				// Skip components without bound states (bound state index bndIdx is not advanced)
				if (_nBoundStates[j] == 0)
					continue;

				// dres_i / dq_j^A
				jac[bndIdx2 - bndIdx] = factor / static_cast<double>(qMax1[j]);
				// Getting to q_j: -bndIdx takes us to q_0, another +bndIdx2 to q_j. This means jac[bndIdx2 - bndIdx] corresponds to q_j.

				++bndIdx2;
			}

			// Fill dres_i / dq_j^B
			for (int j = 0; j < _nComp; ++j)
			{
				// Skip components without bound states (bound state index bndIdx is not advanced)
				if (_nBoundStates[j] == 0)
					continue;

				// dres_i / dq_j^B
				jac[bndIdx2 - bndIdx] = factor / static_cast<double>(qMax2[j]);
				// Getting to q_j: -bndIdx takes us to q_0, another +bndIdx2 to q_j. This means jac[bndIdx2 - bndIdx] corresponds to q_j.

				++bndIdx2;
			}

			// Add to dres_i / dq_i^B
			jac[0] += kd + static_cast<double>(p.k21[i]);

			// Add to dres_i / dq_i^A
			jac[-static_cast<int>(_numBindingComp)] -= static_cast<double>(p.k12[i]) * static_cast<double>(qMax1[i]) * qSum;  // last summand by product rule

			// Advance to next equation and Jacobian row
			++bndIdx;
			++jac;
		}
	}
};


typedef MultiComponentSpreadingBindingBase<SpreadingParamHandler> MultiComponentSpreadingBinding;
typedef MultiComponentSpreadingBindingBase<ExtSpreadingParamHandler> ExternalMultiComponentSpreadingBinding;

namespace binding
{
	void registerMultiComponentSpreadingModel(std::unordered_map<std::string, std::function<model::IBindingModel*()>>& bindings)
	{
		bindings[MultiComponentSpreadingBinding::identifier()] = []() { return new MultiComponentSpreadingBinding(); };
		bindings[ExternalMultiComponentSpreadingBinding::identifier()] = []() { return new ExternalMultiComponentSpreadingBinding(); };
	}
}  // namespace binding

}  // namespace model

}  // namespace cadet
