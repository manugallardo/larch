import numpy as np
import pandas as pd
import xarray as xr
from typing import NamedTuple
from .model import NumbaModel

class _case_slice:

    def __get__(self, obj, objtype=None):
        self.parent = obj
        return self

    def __getitem__(self, idx):
        return type(self.parent)(
            **{
                k: getattr(self.parent, k)[idx] if len(k)==2 else getattr(self.parent, k)
                for k in self.parent._fields
            }
        )


class DataArrays(NamedTuple):
    ch: np.ndarray
    av: np.ndarray
    wt: np.ndarray
    co: np.ndarray
    ca: np.ndarray
    alt_codes: np.ndarray = None
    alt_names: np.ndarray = None

    cs = _case_slice()

    @property
    def alternatives(self):
        if self.alt_codes is not None:
            if self.alt_names is not None:
                return dict(zip(self.alt_codes, self.alt_names))
            else:
                return {i:str(i) for i in self.alt_codes}
        else:
            raise ValueError("alt_codes not defined")


def to_dataset(dataframes):
    caseindex_name = '_caseid_'
    altindex_name = '_altid_'
    from sharrow import Dataset
    from xarray import DataArray
    ds = Dataset()
    if dataframes.data_co is not None:
        caseindex_name = dataframes.data_co.index.name
        ds.update(Dataset.from_dataframe(dataframes.data_co))
    if dataframes.data_ca is not None:
        caseindex_name = dataframes.data_ca.index.names[0]
        altindex_name = dataframes.data_ca.index.names[1]
        ds.update(Dataset.from_dataframe(dataframes.data_ca))
    altnames = dataframes.alternative_names()
    if altnames:
        ds.coords['alt_names'] = DataArray(altnames, dims=(altindex_name,))
    return ds


def prepare_data(datashare, request):
    from sharrow import SharedData, Dataset, DataArray
    model_dataset = Dataset(
        coords=datashare.coords,
    )
    if isinstance(request, NumbaModel):
        request = request.required_data()
    shared_data_ca = SharedData(datashare)
    shared_data_co = SharedData(datashare.drop_dims(
        [i for i in datashare.dims.keys() if i != "_caseid_"]
    ))
    if 'co' in request:
        model_dataset = _prep_co(model_dataset, shared_data_co, request['co'], tag='co')
    if 'ca' in request:
        model_dataset = _prep_ca(model_dataset, shared_data_ca, request['ca'], tag='ca')

    if 'choice_ca' in request:
        model_dataset = _prep_ca(model_dataset, shared_data_ca, [request['choice_ca']], tag='ch', preserve_vars=False)
    if 'choice_co_code' in request:
        choicecodes = datashare[request['choice_co_code']]
        float_dtype = np.float32
        da_ch = DataArray(
            float_dtype(0),
            dims=['_caseid_', '_altid_'],
            coords={
                '_caseid_':model_dataset.coords['_caseid_'],
                '_altid_': model_dataset.coords['_altid_'],
            },
            name='ch',
        )
        for i,a in enumerate(model_dataset.coords['_altid_']):
            da_ch[:, i] = (choicecodes == a)
        model_dataset = model_dataset.merge(da_ch)
    if 'choice_co_vars' in request:
        raise NotImplementedError('choice_co_vars')
    if 'choice_any' in request:
        raise NotImplementedError('choice_any')

    if 'weight_co' in request:
        model_dataset = _prep_co(model_dataset, shared_data_co, [request['weight_co']], tag='wt', preserve_vars=False)

    if 'avail_ca' in request:
        model_dataset = _prep_ca(model_dataset, shared_data_ca, [request['avail_ca']], tag='av', preserve_vars=False, dtype=np.int8)
    if 'avail_co' in request:
        raise NotImplementedError('avail_co')
    if 'avail_any' in request:
        raise NotImplementedError('avail_any')

    return model_dataset


def _prep_ca(
        model_dataset,
        shared_data_ca,
        vars_ca,
        tag='ca',
        preserve_vars=True,
        dtype=None,
):
    from sharrow import Dataset, DataArray
    if not isinstance(vars_ca, dict):
        vars_ca = {i:i for i in vars_ca}
    flow = shared_data_ca.setup_flow(vars_ca)
    arr = flow.load(dtype=dtype)
    if preserve_vars or len(vars_ca)>1:
        arr = arr.reshape(
            model_dataset.dims.get('_caseid_'),
            model_dataset.dims.get('_altid_'),
            -1,
        )
        da = DataArray(
            arr,
            dims=['_caseid_', '_altid_', f"var_{tag}"],
            coords={
                '_caseid_':model_dataset.coords['_caseid_'],
                '_altid_': model_dataset.coords['_altid_'],
                f"var_{tag}": list(vars_ca.keys()),
            },
            name=tag,
        )
    else:
        arr = arr.reshape(
            model_dataset.dims.get('_caseid_'),
            model_dataset.dims.get('_altid_'),
        )
        da = DataArray(
            arr,
            dims=['_caseid_', '_altid_'],
            coords={
                '_caseid_':model_dataset.coords['_caseid_'],
                '_altid_': model_dataset.coords['_altid_'],
            },
            name=tag,
        )
    return model_dataset.merge(da)


def _prep_co(model_dataset, shared_data_co, vars_co, tag='co', preserve_vars=True, dtype=None):
    from sharrow import DataArray
    if not isinstance(vars_co, dict):
        vars_co = {i: i for i in vars_co}
    flow = shared_data_co.setup_flow(vars_co)
    arr = flow.load(dtype=dtype)
    if preserve_vars or len(vars_co)>1:
        arr = arr.reshape(
            model_dataset.dims.get('_caseid_'),
            -1,
        )
        da = DataArray(
            arr,
            dims=['_caseid_', f"var_{tag}"],
            coords={
                '_caseid_':model_dataset.coords['_caseid_'],
                f"var_{tag}": list(vars_co.keys()),
            },
            name=tag,
        )
    else:
        arr = arr.reshape(
            model_dataset.dims.get('_caseid_'),
        )
        da = DataArray(
            arr,
            dims=['_caseid_'],
            coords={
                '_caseid_':model_dataset.coords['_caseid_'],
            },
            name=tag,
        )
    return model_dataset.merge(da)

