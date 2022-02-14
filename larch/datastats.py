import altair as alt
import numpy as np
import pandas as pd
import seaborn as sns
import xarray as xr
import xmle
from scipy.stats import gaussian_kde

from .dataset import DataArray, Dataset
from .util.bounded_kde import bounded_gaussian_kde, BoundedKDE, weighted_sample_std


def fair_range(x, buffering=0.1):
    lo, hi = np.percentile(x, [1, 99])
    buffer = (hi - lo) * buffering
    return lo - buffer, hi + buffer


def scotts_factor(self, n, d=1):
    """
    Compute Scott's factor.

    Parameters
    ----------
    n : numeric
        Effective number of data points, either the cardinality
        of the array or the total weight.
    d : int, default 1
        Number of dimensions.

    Returns
    -------
    float
    """
    return np.power(n, -1.0 / (d + 4))


def silverman_factor(self, n, d=1):
    """
    Compute the Silverman factor.

    Parameters
    ----------
    n : numeric
        Effective number of data points, either the cardinality
        of the array or the total weight.
    d : int, default 1
        Number of dimensions.

    Returns
    -------
    float
    """
    return np.power(n * (d + 2.0) / 4.0, -1.0 / (d + 4))




def idca_statistics_data(
    var,
    bw_method="scott",
    choice=None,
    avail=None,
    wgt=None,
    scale_ch=True,
    bins=10,
    resolution=10,
):
    name = var.name or "Variable"
    if avail is None:
        avail = DataArray(xr.ones_like(var, dtype=np.int8))
    if wgt is None:
        wgt = np.ones(var.n_cases, dtype=np.float32)
    else:
        wgt = np.asarray(wgt)
    n_alts = var.n_alts
    n = np.prod(var.shape)
    if bw_method == "scott":
        bandwidth = scotts_factor(n, 1)
    elif bw_method == "silverman":
        bandwidth = scotts_factor(n, 1)
    elif np.isscalar(bw_method) and not isinstance(bw_method, str):
        bandwidth = bw_method
    else:
        raise ValueError("`bw_method` should be 'scott', 'silverman', or a scalar")
    stats = Dataset({"mean": var.mean(var.CASEID), "std": var.std(var.CASEID),})
    data = {}
    data_ch = {}
    data_av = {}
    for k in range(n_alts):
        if stats["std"][k] > 0:
            if "alt_names" in var.coords:
                key = str(var.coords["alt_names"][k].data)
            elif var.ALTID in var.coords:
                key = str(var.coords[var.ALTID][k].data)
            else:
                key = str(k)
            data[key] = var.isel({var.ALTID: k}).values
            if choice is not None:
                data_ch[key] = choice.isel({choice.ALTID: k}).values
            data_av[key] = avail.isel({avail.ALTID: k}).values
    hist_overall, bins = np.histogram(
        var.data, bins=bins, weights=avail.data * wgt[:, None]
    )
    kde_overall = BoundedKDE(
        var.data, weights=avail.data * wgt[:, np.newaxis]
    )
    # eff_wgts = avail.data * wgt[:, np.newaxis]
    # bandwidth = (
    #         weighted_sample_std(var.data.reshape(-1), eff_wgts.reshape(-1), ddof=1.0)
    #         * np.power(eff_wgts.sum(), -1.0 / 5)
    # )
    x = np.linspace(bins[0], bins[-1], resolution * len(bins))
    hist = {}
    hist_ch = {}
    kde = {}
    kde_ch = {}
    for k, v in data.items():
        hist[k] = np.append(
            np.repeat(
                np.histogram(v, bins=bins, weights=data_av[k] * wgt)[0], resolution
            ),
            0,
        )
        kde[k] = BoundedKDE(v, bw_method=kde_overall, weights=data_av[k] * wgt)(
            x
        )
    if choice is not None:
        for k, v in data.items():
            hist_ch[k] = np.append(
                np.repeat(
                    np.histogram(v, bins=bins, weights=data_ch[k] * data_av[k] * wgt)[0], resolution
                ),
                0,
            )
            y = BoundedKDE(
                v, bw_method=kde_overall, weights=data_ch[k] * data_av[k] * wgt
            )(x)
            if scale_ch:
                y = y * (data_ch[k] * wgt).sum() / (data_av[k] * wgt).sum()
            kde_ch[k] = y
        plot_data = pd.concat(
            [pd.DataFrame(hist), pd.DataFrame(hist_ch), pd.DataFrame(kde), pd.DataFrame(kde_ch)],
            axis=1,
            keys=["Frequency", "FrequencyChosen", "Density", "DensityChosen"],
        )
        plot_data = (
            plot_data.set_index(x)
            .stack()
            .rename_axis(index=(name, "Alternative"))
            .reset_index()
        )
    else:
        kde_ch = None
        plot_data = (
            pd.DataFrame(kde, index=x)
            .stack()
            .rename("Density")
            .rename_axis(index=(name, "Alternative"))
            .reset_index()
        )
    return stats, plot_data


def idca_statistics(
    var,
    bw_method="scott",
    choice=None,
    avail=None,
    wgt=None,
    scale_ch=True,
    bins=10,
    resolution=100,
):
    stats, plot_data = idca_statistics_data(
        var,
        bw_method=bw_method,
        choice=choice,
        avail=avail,
        wgt=wgt,
        scale_ch=scale_ch,
        bins=bins,
        resolution=resolution,
    )
    name = var.name or "Variable"
    selection = alt.selection_single(fields=["Alternative"], bind="legend", empty="all")
    selection2 = alt.selection_single(
        fields=["Alternative"], bind="legend", empty="none"
    )
    ax = alt.Chart(plot_data)

    # A dropdown filter
    # options = [None] + list(str(i.data) for i in var['alt_names'])
    # options_dropdown = alt.binding_select(options=options)
    # options_select = alt.selection_single(fields=['Alternative'], bind=options_dropdown, name="Alternative", empty="all")

    frequency = (
        ax.mark_line(interpolate="step",)
        .encode(
            x=name,
            y="Frequency",
            color=alt.Color("Alternative", legend=alt.Legend(title="Alternative", orient="left")),
            # strokeDash="Alternative",
            # tooltip="Alternative",
            opacity=alt.condition(selection, alt.value(1), alt.value(0.0)),
        )
        .add_selection(selection)
    )
    density = (
        ax.mark_line()
        .encode(
            x=name,
            y=alt.Y("Density", title="Density"),
            color=alt.Color("Alternative", legend=None),
            # strokeDash=alt.Color("Alternative", legend=None),
            # tooltip="Alternative",
            opacity=alt.condition(selection2, alt.value(1), alt.value(0.0)),
        )
        .add_selection(selection2)
        .transform_filter(selection2)
    )
    if choice is not None:
        f_areas = (
            ax.mark_area(interpolate="step",)
            .encode(
                x=name,
                y=alt.Y('FrequencyChosen', title="Frequency"),
                color=alt.Color("Alternative", legend=None),
                # strokeDash="Alternative",
                # tooltip="Alternative",
                opacity=alt.condition(
                    selection2, alt.value(0.75), alt.value(0.0)
                ),
            )
            .transform_filter(selection2)
        )
        frequency = frequency + f_areas
        d_areas = (
            ax.mark_area()
            .encode(
                x=name,
                y=alt.Y('DensityChosen', title="Density"),
                color=alt.Color("Alternative", legend=None),
                # strokeDash="Alternative",
                # tooltip="Alternative",
                opacity=alt.condition(
                    selection2, alt.value(1 if scale_ch else 0.75), alt.value(0.0)
                ),
            )
            .transform_filter(selection2)
        )
        density = density + d_areas

    chart = (
        frequency.properties(height=160)
        & density.properties(height=90)
    )

    return chart, stats
