/*
 *  elm_model.cpp
 *
 *  Copyright 2007-2015 Jeffrey Newman
 *
 *  Larch is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  Larch is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with Larch.  If not, see <http://www.gnu.org/licenses/>.
 *  
 */


#include <cstring>
#include "elm_model2.h"
#include "elm_sql_scrape.h"
#include "elm_names.h"
#include <iostream>

#include "elm_parameter2.h"

using namespace etk;
using namespace elm;
using namespace std;



etk::strvec elm::__identify_needs(const ComponentList& Input_List)
{
	etk::strvec u_ca;
	
	for (unsigned b=0; b<Input_List.size(); b++) {
		u_ca.push_back_if_unique(Input_List[b].data_name);
	}
	
	return u_ca;
}


void __check_validity_of_needs(const etk::strvec& needs, Facet*	_Data, int k, etk::logging_service* msg)
{
	if (!_Data) return;

	if (k & IDCA) {
		for (auto i=needs.begin(); i!=needs.end(); i++) {
			BUGGER_(msg, "checking for validity of "<<*i<<" in idCA data");
			_Data->check_ca(*i);
		}
	} else if (k & IDCO) {
		for (auto i=needs.begin(); i!=needs.end(); i++) {
			BUGGER_(msg, "checking for validity of "<<*i<<" in idCO data");
			_Data->check_co(*i);
		}
	}
}


void _setUp_linear_data_and_params
(	ParameterList&			self
 ,	Facet*					_Data
 ,	VAS_System&				Xylem
 ,	ComponentList&			Input_UtilityCA
 ,	ComponentList&			Input_UtilityCO
 ,	paramArray&				Params_UtilityCA
 ,	paramArray&				Params_UtilityCO
 ,	etk::logging_service*	msg
)
{
	size_t slot, slot2;


	// utility.ca //
	// First, populate the data_port
	etk::strvec u_ca = __identify_needs(Input_UtilityCA);
	__check_validity_of_needs(u_ca, _Data, IDCA, msg);
	
	// Second, resize the paramArray
	BUGGER_(msg, "setting Params_?CA size to ("<<u_ca.size()<<")");
	Params_UtilityCA.resize(u_ca.size());
	
	// Third, populate the paramArray
	for (unsigned b=0; b<Input_UtilityCA.size(); b++) {
		slot = u_ca.push_back_if_unique(Input_UtilityCA[b].data_name);
		Params_UtilityCA(slot) = self._generate_parameter(Input_UtilityCA[b].param_name,Input_UtilityCA[b].multiplier);
	}
	
	
	
	// utility.co //
	// First, populate the data_port
	etk::strvec u_co = __identify_needs(Input_UtilityCO);
	__check_validity_of_needs(u_co, _Data, IDCO, msg);

	// Second, resize the paramArray
	auto s = _Data ? _Data->nAlts() : Xylem.n_elemental();
	BUGGER_(msg, "setting Params_?CO size to ("<<u_co.size()<<","<< s <<")");
	Params_UtilityCO.resize(u_co.size(), s);
	
	// Third, populate the paramArray
	for (unsigned b=0; b<Input_UtilityCO.size(); b++) {
		BUGGER_(msg, "setting Params_?CO b="<<b);
		slot = u_co.push_back_if_unique(Input_UtilityCO[b].data_name);
		if (!Input_UtilityCO[b]._altname.empty()) {
			slot2 = Xylem.slot_from_name(Input_UtilityCO[b]._altname);
		} else {
			if (Input_UtilityCO[b]._altcode==cellcode_empty) {
				OOPS("utilityco input does not specify an alternative.\n"
					 "Inputs in the utilityco space need to identify an alternative.");
			}
			slot2 = Xylem.slot_from_code(Input_UtilityCO[b]._altcode);
		}
		Params_UtilityCO(slot,slot2) = self._generate_parameter(Input_UtilityCO[b].param_name, Input_UtilityCO[b].multiplier);
	}
	
	BUGGER_(msg, "_setUp_linear_data_and_params complete");
	
}



void _setUp_linear_data_and_params_edges
(	ParameterList&			self
 ,	VAS_System&				Xylem
 ,	ComponentList&			Input_Alloc
 ,	paramArray&				Params_Alloc
 ,	etk::logging_service*	msg
)
{
	size_t slot, slot2;

	
	
	// idco //
	etk::strvec u_co = __identify_needs(Input_Alloc);

	// resize the paramArray
	auto s = Xylem.n_compet_alloc();
	BUGGER_(msg, "setting Params_?CO size to ("<<u_co.size()<<","<< s <<")");
	Params_Alloc.resize(u_co.size(), s);
	
	// Third, populate the paramArray
	for (unsigned b=0; b<Input_Alloc.size(); b++) {
		BUGGER_(msg, "setting Params_?CO b="<<b);
		slot = u_co.push_back_if_unique(Input_Alloc[b].data_name);
		
		auto edge = Xylem.edge_from_codes(Input_Alloc[b]._upcode, Input_Alloc[b]._dncode);
		if (!edge) {
			OOPS("allocation input does not specify a valid network link.");
		}
		slot2 = edge->alloc_slot();
		
		Params_Alloc(slot,slot2) = self._generate_parameter(Input_Alloc[b].param_name, Input_Alloc[b].multiplier);
	}
	
	BUGGER_(msg, "_setUp_linear_data_and_params_edges complete");
	
}




void elm::Model2::_setUp_utility_data_and_params()
{
	BUGGER(msg) << "--Params_Utility--\n";

	_setUp_linear_data_and_params
	(	*this
	 ,	_Data
	 ,	Xylem
	 ,	Input_Utility.ca
	 ,	Input_Utility.co
	 ,	Params_UtilityCA
	 ,	Params_UtilityCO
	 ,	&msg
	);

	BUGGER(msg) << "Params_UtilityCA \n" << Params_UtilityCA.__str__();
	BUGGER(msg) << "Params_UtilityCO \n" << Params_UtilityCO.__str__();

}

void elm::Model2::_setUp_samplefactor_data_and_params()
{
	BUGGER(msg) << "--Params_Sampling--\n";

	_setUp_linear_data_and_params
	(	*this
	 ,	_Data
	 ,	Xylem
	 ,	Input_Sampling.ca
	 ,	Input_Sampling.co
	 ,	Params_SamplingCA
	 ,	Params_SamplingCO
	 ,	&msg
	);

	BUGGER(msg) << "Params_SamplingCA \n" << Params_SamplingCA.__str__();
	BUGGER(msg) << "Params_SamplingCO \n" << Params_SamplingCO.__str__();

}




std::string __GetWeight(const std::string& varname, bool reweight, Facet* _Data) {
	std::string w;	
	if (reweight) {
		std::ostringstream sql1;
		sql1 << "SELECT SUM("<<varname<<") FROM " << _Data->tbl_idco();
		double average_weight = _Data->sql_statement(sql1)->execute()->getDouble(0); 
		
		average_weight /= _Data->nCases();
		ostringstream sql2;
		sql2 << "("<<varname<<")/"<<average_weight;
		w = (sql2.str());
	} else {
		w = (varname);
	}	
	return w;
}

//void elm::Model2::_setUp_weight_data()
//{
//	if (!_Data) OOPS("A database must be linked to this model to do this.");
//
//	if (_Data->unweighted()) {
//		WARN(msg) << "No weights specified, defaulting to weight=1 for all cases.";
//	} else {
//		Data_Weight = _Data->ask_weight();
//		weight_scale_factor = 1.0;
//	}
//
//}

void elm::Model2::auto_rescale_weights(const double& mean_weight)
{
	if (Data_Weight) {

		double current_total = Data_Weight->_repository.sum();
		double needed_scale_factor = (mean_weight*Data_Weight->_repository.size())/current_total;
		Data_Weight_rescaled = boosted::make_shared<elm::darray>(*Data_Weight,needed_scale_factor);
		weight_scale_factor = needed_scale_factor;
		INFO(msg) << "automatically rescaled weights (total initial weight "<<current_total
					<<" scaled by "<<weight_scale_factor<<" across "<<Data_Weight->nCases()
					<<" cases)";
	}
}

void elm::Model2::restore_scale_weights()
{
	Data_Weight_rescaled.reset();
}



void elm::Model2::scan_for_multiple_choices()
{
	BUGGER(msg) << "Scanning choice data for instances of multiple or non-unit choice....";
	
	// multichoice
	if (Data_MultiChoice.size() != Data_Choice->nCases()) {
		Data_MultiChoice.resize(Data_Choice->nCases());
	}
	
	for (unsigned c=0;c<Data_Choice->nCases();c++) {
		size_t m=c;
		int found=0;
		double sum = 0;
		for (size_t a=0;a<nElementals;a++) {
			if (Data_Choice->value(c,a,0)) {
				found++;
				sum += Data_Choice->value(c,a,0);
			}
		}
		if (found>1 || sum != 1.0) {
			Data_MultiChoice.input(true,m);
		} else {
			Data_MultiChoice.input(false,m);
		}
	}
}


void elm::Model2::setUp(bool and_load_data)
{
	INFO(msg) << "Setting up the model...";
	
	if (is_provisioned()!=1) {
		OOPS("data not provisioned");
	}

//	BUGGER(msg) << "Setting up the model...";
	if (_is_setUp>=2 || (_is_setUp>=1 && !and_load_data)) {
		BUGGER(msg) << "The model is already set up.";
		return;
	}

	BUGGER(msg) << "Rebuilding Xylem network...";
	elm::cellcode root = Xylem.root_cellcode();
	Xylem.regrow( &Input_LogSum, &Input_Edges, _Data, &root, &msg );
	
	if (Xylem.n_branches() > 0) {
		BUGGER(msg) << "Setting model features to include nesting.";
		features |= MODELFEATURES_NESTING;
	}
	
	BUGGER(msg) << "Setting up utility parameters...";
	_setUp_utility_data_and_params();
	if (features & MODELFEATURES_NESTING) {
		elm::cellcode root = Xylem.root_cellcode();
		Xylem.touch();
		Xylem.regrow( &Input_LogSum, &Input_Edges, _Data, &root, &msg );
		_setUp_NL();
	} else {
		_setUp_MNL();
	}
	if (and_load_data) scan_for_multiple_choices();

	
	if (Input_Sampling.ca.size() || Input_Sampling.co.size()) {
		_setUp_samplefactor_data_and_params();
	}
	_setUp_coef_and_grad_arrays();
	
	if (features & MODELFEATURES_NESTING) {
		Xylem.repoint_parameters(*Coef_LogSum, NULL);
		elm::cellcode root = Xylem.root_cellcode();
		Xylem.regrow( &Input_LogSum, &Input_Edges, _Data, &root, &msg );
	}
	
	pull_coefficients_from_freedoms();
	
	BUGGER(msg) << "Params_UtilityCA \n" << Params_UtilityCA.__str__();
	BUGGER(msg) << "Params_UtilityCO \n" << Params_UtilityCO.__str__();
	
	_is_setUp = 1;
	if (and_load_data) _is_setUp = 2;

	
}


void elm::Model2::tearDown()
{
	_is_setUp = 0;
//	if (Data_UtilityCA  ) { Data_UtilityCA   ->decref(); Data_UtilityCA=nullptr;}
//	if (Data_UtilityCO  ) { Data_UtilityCO   ->decref(); Data_UtilityCO=nullptr;}
//	if (Data_QuantityCA ) { Data_QuantityCA  ->decref(); Data_QuantityCA=nullptr;}
//	if (Data_QuantLogSum) { Data_QuantLogSum ->decref(); Data_QuantLogSum=nullptr;}
//	if (Data_LogSum     ) { Data_LogSum      ->decref(); Data_LogSum=nullptr;}
//	if (Data_Avail      ) { Data_Avail       ->decref(); Data_Avail=nullptr;}
	
	probability_dispatcher.reset();
	gradient_dispatcher.reset();
	loglike_dispatcher.reset();
}












std::string elm::Model2::_subprovision(const std::string& name, boosted::shared_ptr<const darray>& storage,
								const std::map< std::string, boosted::shared_ptr<const darray> >& input,
								const std::map<std::string, darray_req>& need,
								std::map<std::string, size_t>& ncases)
{

	auto i = input.find(name);
	auto n = need.find(name);
	if (i!=input.end()) {
		// This pointer is provisioned
		if (n->second.satisfied_by(&*i->second)<0) {
			// if it does not satisty the need, add to the exception
			storage = nullptr;
			return cat("\ndata for ",name," is provisioned by an array that does not satisfy the need");
		}
		storage = i->second;
		ncases[name] = storage->nCases();
	} else {
		// This pointer is not provisioned
		storage = nullptr;
		if (n != need.end()) {
			// if it is needed, add to the exception
			return cat("\ndata for ",name," is needed but not provisioned");
		}
	}
	return "";
}




void elm::Model2::provision(const std::map< std::string, boosted::shared_ptr<const darray> >& input)
{
	BUGGER(msg) << "Provisioning model data...";
	
	std::string ret = "";
	
	std::map<std::string, darray_req> need = needs();
	std::map<std::string, size_t> ncases;
	
	ret += _subprovision("UtilityCA", Data_UtilityCA, input, need, ncases);
	ret += _subprovision("UtilityCO", Data_UtilityCO, input, need, ncases);
	ret += _subprovision("SamplingCA", Data_SamplingCA, input, need, ncases);
	ret += _subprovision("SamplingCO", Data_SamplingCO, input, need, ncases);

	ret += _subprovision("Avail",  Data_Avail , input, need, ncases);
	ret += _subprovision("Choice", Data_Choice, input, need, ncases);
	if (Data_Choice) scan_for_multiple_choices();
	ret += _subprovision("Weight", Data_Weight, input, need, ncases);

	if (!ret.empty()) {
		OOPS("provisioning error:",ret);
	}
	
	auto caseiter = ncases.begin();
	size_t nc = caseiter->second;
	for (; caseiter!=ncases.end(); caseiter++) {
		if (nc != caseiter->second) {
			OOPS("provisioning error: inconsistent numbers or cases");
		}
	}
	
	nCases = nc;
}

std::map<std::string, darray_req> elm::Model2::needs() const
{
	std::map<std::string, darray_req> requires;
	
	etk::strvec u_ca = __identify_needs(Input_Utility.ca);
	if (u_ca.size()) {
		requires["UtilityCA"] = darray_req (3,NPY_DOUBLE,Xylem.n_elemental());
		requires["UtilityCA"].set_variables(u_ca);
	}
	
	etk::strvec u_co = __identify_needs(Input_Utility.co);
	if (u_co.size()) {
		requires["UtilityCO"] = darray_req (2,NPY_DOUBLE);
		requires["UtilityCO"].set_variables(u_co);
	}


	etk::strvec s_ca = __identify_needs(Input_Sampling.ca);
	if (s_ca.size()) {
		requires["SamplingCA"] = darray_req (3,NPY_DOUBLE,Xylem.n_elemental());
		requires["SamplingCA"].set_variables(s_ca);
	}
	
	etk::strvec s_co = __identify_needs(Input_Sampling.co);
	if (s_co.size()) {
		requires["SamplingCO"] = darray_req (2,NPY_DOUBLE);
		requires["SamplingCO"].set_variables(s_co);
	}
	
	
	requires["Avail"] = darray_req (3,NPY_BOOL);
	requires["Weight"] = darray_req (2,NPY_DOUBLE);
	requires["Choice"] = darray_req (3,NPY_DOUBLE);
	
	return requires;
}



#define MISSING_BUT_NEEDED 0x1
#define GIVEN_BUT_WRONG    0x2
#define NOT_NEEDED         0x0
#define GIVEN_CORRECTLY    0x0


int elm::Model2::_is_subprovisioned(const std::string& name, const elm::darray_ptr& arr, const std::map<std::string, darray_req>& requires, const bool& ex) const
{
	auto i = requires.find(name);
	if (i!=requires.end()) {
		if (!arr) {
			if (ex) {
				OOPS(name," is provisioned incorrectly, needs <",i->second.__str__(),"> but not provided");
			} else {
				return MISSING_BUT_NEEDED;
			}
		}
		if (i->second.satisfied_by(&*arr)==0) {
			return GIVEN_CORRECTLY;
		} else {
			if (ex) {
				OOPS(name," is provisioned incorrectly, needs <",i->second.__str__(),"> but provides <",arr->__str__(),">");
			} else {
				return GIVEN_BUT_WRONG;
			}
		}
	} else {
		return NOT_NEEDED;
	}
}

int elm::Model2::is_provisioned(bool ex) const
{
	std::map<std::string, darray_req> requires = needs();
	
	int i = 0;
	i |= _is_subprovisioned("UtilityCA", Data_UtilityCA, requires, ex);
	i |= _is_subprovisioned("UtilityCO", Data_UtilityCO, requires, ex);
	i |= _is_subprovisioned("SamplingCA", Data_SamplingCA, requires, ex);
	i |= _is_subprovisioned("SamplingCO", Data_SamplingCO, requires, ex);
	
	i |= _is_subprovisioned("Avail", Data_Avail, requires, ex);
	i |= _is_subprovisioned("Weight", Data_Weight, requires, ex);
	i |= _is_subprovisioned("Choice", Data_Choice, requires, ex);
	
	if (i & GIVEN_BUT_WRONG) {
		return -1;
	}
	if (i & MISSING_BUT_NEEDED) {
		return 0;
	}
	return 1;
}

const elm::darray* elm::Model2::Data(const std::string& label)
{
	if (label=="UtilityCA") return Data_UtilityCA ?   (&*Data_UtilityCA) : nullptr;
	if (label=="UtilityCO") return Data_UtilityCO ?   (&*Data_UtilityCO) : nullptr;
	if (label=="SamplingCA") return Data_SamplingCA ? (&*Data_SamplingCA) : nullptr;
	if (label=="SamplingCO") return Data_SamplingCO ? (&*Data_SamplingCO) : nullptr;

	if (label=="Avail" ) return Data_Avail ?  (&*Data_Avail ) : nullptr;
	if (label=="Choice") return Data_Choice ? (&*Data_Choice) : nullptr;
	if (label=="Weight") return Data_Weight ? (&*Data_Weight) : nullptr;

	OOPS(label, " is not a valid label for model data");
	
}

