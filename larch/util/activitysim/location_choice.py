import os
import numpy as np
import pandas as pd
import yaml
from typing import Collection
from .. import Dict

from .general import (
	remove_apostrophes,
	construct_nesting_tree,
	linear_utility_from_spec,
	explicit_value_parameters,
	apply_coefficients,
	cv_to_ca,
)
from ... import Model, DataFrames, P, X



def size_coefficients_from_spec(size_spec):
	size_coef = size_spec.stack().reset_index()
	size_coef.index = size_coef.iloc[:, 0] + "_" + size_coef.iloc[:, 1]
	size_coef = size_coef.loc[size_coef.iloc[:, 2] > 0]
	size_coef['constrain'] = 'F'
	one_each = size_coef.groupby('segment').first().reset_index()
	size_coef.loc[one_each.iloc[:, 0] + "_" + one_each.iloc[:, 1], 'constrain'] = 'T'
	size_coef = size_coef.iloc[:, 2:]
	size_coef.columns = ['value', 'constrain']
	size_coef.index.name = 'coefficient_name'
	size_coef['value'] = np.log(size_coef['value'])
	return size_coef


def location_choice_model(
		model_selector='workplace',
		edb_directory="output/estimation_data_bundle/{model_selector}_location/",
		coefficients_file="{model_selector}_location_coefficients.csv",
		spec_file="{model_selector}_location_SPEC.csv",
		size_spec_file="{model_selector}_location_size_terms.csv",
		alt_values_file="{model_selector}_location_alternatives_combined.csv",
		chooser_file="{model_selector}_location_choosers_combined.csv",
		settings_file="{model_selector}_location_model_settings.yaml",
		landuse_file="{model_selector}_location_landuse.csv",
		return_data=False,
):
	edb_directory = edb_directory.format(model_selector=model_selector)
	def _read_csv(filename, **kwargs):
		filename = filename.format(model_selector=model_selector)
		return pd.read_csv(os.path.join(edb_directory, filename), **kwargs)

	coefficients = _read_csv(
		coefficients_file,
		index_col='coefficient_name',
	)
	spec = _read_csv(spec_file)
	alt_values = _read_csv(alt_values_file)
	chooser_data = _read_csv(chooser_file)
	landuse = _read_csv(landuse_file, index_col='zone_id')
	size_spec = _read_csv(size_spec_file)

	settings_file = settings_file.format(model_selector=model_selector)
	with open(os.path.join(edb_directory, settings_file), "r") as yf:
		settings = yaml.load(
			yf,
			Loader=yaml.SafeLoader,
		)

	size_spec = size_spec \
		.query(f"model_selector == '{model_selector}'") \
		.drop(columns='model_selector') \
		.set_index('segment')
	size_spec = size_spec.loc[:, size_spec.max() > 0]

	size_coef = size_coefficients_from_spec(size_spec)

	# Remove shadow pricing and pre-existing size expression for re-estimation
	spec = spec \
		.set_index('Label') \
		.drop(index=['util_size_variable', 'util_utility_adjustment']) \
		.reset_index()

	m = Model()
	m.utility_ca = linear_utility_from_spec(
		spec, x_col='Label', p_col='coefficient',
		ignore_x=('local_dist',),
	)
	m.quantity_ca = sum(
		P(f"{i}_{q}") * X(q) * X(f"income_segment=={settings['SEGMENT_IDS'][i]}")
		for i in size_spec.index
		for q in size_spec.columns
	)

	apply_coefficients(coefficients, m)
	apply_coefficients(size_coef, m, minimum=-6, maximum=6)

	x_co = chooser_data.set_index('person_id')#.rename(columns={'TAZ': 'HOMETAZ'})
	x_ca = cv_to_ca(
		alt_values.set_index(['person_id', 'variable'])
	)

	# Remove choosers with invalid observed choice
	workplace_tazs = landuse[landuse['TOTEMP'] > 0].index
	x_co = x_co[x_co['override_choice'].isin(workplace_tazs)]
	x_ca = x_ca[x_ca.index.get_level_values('person_id').isin(x_co.index)]

	x_ca_1 = pd.merge(x_ca, landuse, on='zone_id', how='left')
	x_ca_1.index = x_ca.index

	# Availability of choice zones
	av = x_ca_1['util_no_attractions'].apply(lambda x: False if x == 1 else True)

	d = DataFrames(
		co=x_co,
		ca=x_ca_1,
		av=av,
	)
	m.dataservice = d
	m.choice_co_code = 'override_choice'

	if return_data:
		return m, Dict(
			alt_values=alt_values,
			chooser_data=chooser_data,
			coefficients=coefficients,
			landuse=landuse,
			spec=spec,
			size_spec=size_spec,
		)

	return m
