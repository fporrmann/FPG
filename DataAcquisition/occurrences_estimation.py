import numpy as np
import math
import copy
from scipy.stats import binom
from scipy.special import binom as binom_coeff
import quantities as pq
import os


# Function to create new folders
def mkdirp(directory):
    if not os.path.isdir(directory):
        try:
            os.mkdir(directory)
        except FileExistsError:
            pass


# Function to split path to single folders
def split_path(path):
    folders = []
    while 1:
        path, folder = os.path.split(path)
        if folder != "":
            folders.append(folder)
        else:
            if path != "":
                folders.append(path)
            break
    folders.reverse()
    return folders


def create_rate_dict(session,
                     ep,
                     trialtype,
                     binsize):
    """
    Function to create rate dictionary in order to estimate the
    expected number of occurrences of a pattern of defined size
    under the Poisson assumption.

    Parameters
    ----------
    session: string
        recording session
    ep: str
        epoch of the trial taken into consideration
    trialtype: str
        trialtype taken into consideration
    binsize: pq.quantities
        binsize of the spade analysis
    process: str
        model (point process) being analysed and estimated

    Returns
    -------
        dictionary of rates:
        {'rates': sorted rates (in decreased order), 'n_bins': n_bins,
                  'rates_ordered_by_neuron': rates ordered by neuron id}
    """
    data_path = f'./data/{session}/'
    sts_units = np.load(data_path + 'spiketrains_' +
                        ep + '_' + trialtype + '.npy',
                        allow_pickle=True)
    length_data = sts_units[0].t_stop
    # Total number of bins
    n_bins = int(length_data / binsize)
    # Compute list of average firing rate
    rates = []
    # Loop over neurons
    for sts in sts_units:
        spike_count = len(sts)
        rates.append(spike_count / float(length_data))
    sorted_rates = sorted(rates)
    rates_dict = {'rates': sorted_rates, 'n_bins': n_bins,
                  'rates_ordered_by_neuron': rates}
    return rates_dict


def _storing_initial_parameters(param_dict,
                                session,
                                context,
                                job_counter,
                                binsize,
                                unit,
                                ep,
                                tt,
                                min_spikes,
                                max_spikes):
    param_dict[session][context][job_counter] = {}
    param_dict[session][context][job_counter]['trialtype'] = tt
    param_dict[session][context][job_counter]['binsize'] = (
            binsize * pq.s).rescale(unit)
    param_dict[session][context][job_counter]['epoch'] = ep
    param_dict[session][context][job_counter]['min_spikes'] = min_spikes
    param_dict[session][context][job_counter]['max_spikes'] = max_spikes
    return param_dict


def _storing_remaining_parameters(param_dict,
                                  session,
                                  context,
                                  job_counter,
                                  percentile_poiss,
                                  percentile_rates,
                                  winlen,
                                  abs_min_spikes):
    param_dict[session][context][job_counter][
        'percentile_poiss'] = percentile_poiss
    param_dict[session][context][job_counter][
        'percentile_rates'] = percentile_rates
    param_dict[session][context][job_counter][
        'winlen'] = winlen
    param_dict[session][context][job_counter][
        'abs_min_occ'] = abs_min_occ
    param_dict[session][context][job_counter][
        'abs_min_spikes'] = abs_min_spikes
    return param_dict


def estimate_number_occurrences(sessions,
                                epochs,
                                trialtypes,
                                binsize,
                                abs_min_spikes,
                                abs_min_occ,
                                winlen,
                                percentile_poiss,
                                percentile_rates,
                                unit):
    """
    Function estimating the number of occurrences of a random pattern, given
    its size and a percentile of the rate distribution across all neurons,
    under the hypothesis of independence of all neurons and poisson
    distribution of the spike trains.
    The estimation of the number of occurrences is needed just as a lower bound
    for the patterns searched by SPADE. Patterns of a fixed size with a lower
    number of occurrences than the ones estimated by this function
    are automatically left out from the search.
    This number is estimated for all sizes until size 10.
    It also saved a dictionary containing all parameters of the analysis, such
    that all results can be identified from it.

    Parameters
    ----------
    sessions: list of strings
        sessions being analyzed
    epochs: list of string
        epochs of the trials being analyzed
    trialtype: list of strings
        trialtypes being analyzed
    sep: pq.quantities
        separation time between trials
    dither: pq.quantities
        dithering parameter of the surrogate generation
    binsize: pq.quantities
        binsize of the analysis
    winlen: int
        window length of the spade analysis
    abs_min_spikes: int
        minimum number of spikes for a pattern to be detected
    abs_min_occ: int
        minimum number of occurrences for a pattern to be detected
    percentile_poiss: int
        Percentile of Poisson pattern count to set minimum occ (int between 0
        and 100)
    percentile_rates: int
        the percentile of the rate distribution to use to compute min occ (int
        between 0 and 100)
    unit: str
        unit of the analysis

    Returns
    -------
    param_dict: dict
        dictionary containing the input parameters and the estimated number of
        occurrences to be used in the fpgrowth analysis

    """
    # Computing the min_occ for given a pattern size (min_spikes)
    param_dict = {}
    for session in sessions:
        param_dict[session] = {}
        # Total number of jobs
        job_counter = 0
        print('session: ', session)
        # For each epoch computation of min_occ relative to min_spikes
        for ep in epochs:
            print('epoch: ', ep)
            # Storing parameters for each trial type
            for tt in trialtypes:
                rates_dict = \
                    create_rate_dict(session=session,
                                     ep=ep,
                                     trialtype=tt,
                                     binsize=binsize)
                rates = rates_dict['rates']
                n_bins = rates_dict['n_bins']
                rates_by_neuron = np.array(
                    rates_dict['rates_ordered_by_neuron'])
                context = ep + '_' + tt
                param_dict[session][context] = {}
                # setting the min spike to the absolute min spikes value
                min_spikes = abs_min_spikes
                # Computing min_occ for all possible min_spikes until
                # min_occ<abs_min_occ
                min_occ = abs_min_occ + 1
                min_occ_old = n_bins
                while min_occ > abs_min_occ:
                    param_dict = _storing_initial_parameters(
                        param_dict=param_dict,
                        session=session,
                        context=context,
                        job_counter=job_counter,
                        binsize=binsize,
                        unit=unit,
                        ep=ep,
                        tt=tt,
                        min_spikes=min_spikes,
                        max_spikes=min_spikes)
                    # Fixing a reference rate (percentile)
                    rates_nonzero = np.array(rates)[np.array(rates) > 0]
                    rate_ref = np.percentile(rates_nonzero,
                                             percentile_rates)
                    # Probability to have one repetition of the pattern assuming Poiss
                    p = (rate_ref * binsize) ** min_spikes
                    # Computing min_occ as percentile of a binominal(n_bins, p)
                    # Computing total number of possible patterns
                    # (combinations of lags * combinations of neurons)
                    num_combination_patt = (math.factorial(
                        winlen) / math.factorial(
                        winlen - min_spikes - 1)) * (binom_coeff(
                        len(rates_nonzero), min_spikes))
                    min_occ = int(binom.isf((
                                                    1 - percentile_poiss / 100.) / num_combination_patt,
                                            n_bins, p))
                    # Checking if the new min_occ is smaller than the previous one (overcorrected the percentile)
                    if min_occ > min_occ_old:
                        num_combination_patt = num_combination_patt_old
                        min_occ = int(binom.isf((
                                                        1 - percentile_poiss / 100.) / num_combination_patt,
                                                n_bins, p))
                    min_occ_old = copy.copy(min_occ)
                    num_combination_patt_old = copy.copy(
                        num_combination_patt)
                    # Storing max_occ
                    if min_occ <= abs_min_occ:
                        param_dict[session][context][job_counter][
                            'min_occ'] = \
                            abs_min_occ
                        # print(f'{context} {min_spikes=} {abs_min_occ=}')
                    else:
                        param_dict[session][context][job_counter][
                            'min_occ'] = min_occ
                        # print(f'{context} {min_spikes=} {min_occ=}')
                    # Storing remaining parameters
                    param_dict = _storing_remaining_parameters(
                        param_dict=param_dict,
                        session=session,
                        context=context,
                        job_counter=job_counter,
                        percentile_poiss=percentile_poiss,
                        percentile_rates=percentile_rates,
                        winlen=winlen,
                        abs_min_spikes=abs_min_spikes)

                    # Setting parameters for the new iteration
                    min_spikes += 1
                    job_counter += 1
                # additional while loop for patterns up to size 10 to get
                # separate jobs
                while min_spikes < 10:
                    # Storing parameters
                    param_dict = _storing_initial_parameters(
                        param_dict=param_dict,
                        session=session,
                        context=context,
                        job_counter=job_counter,
                        binsize=binsize,
                        unit=unit,
                        ep=ep,
                        tt=tt,
                        min_spikes=min_spikes,
                        max_spikes=min_spikes)
                    # Storing min_occ
                    if min_occ <= abs_min_occ:
                        param_dict[session][context][job_counter][
                            'min_occ'] = \
                            abs_min_occ
                        # print(f'{context} {min_spikes=} {abs_min_occ=}')
                    else:
                        param_dict[session][context][job_counter][
                            'min_occ'] = min_occ
                        # print(f'{context} {min_spikes=} {min_occ=}')
                    param_dict = _storing_remaining_parameters(
                        param_dict=param_dict,
                        session=session,
                        context=context,
                        job_counter=job_counter,
                        percentile_poiss=percentile_poiss,
                        percentile_rates=percentile_rates,
                        winlen=winlen,
                        abs_min_spikes=abs_min_spikes)

                    # Setting parameters for the new iteration
                    min_spikes += 1
                    job_counter += 1
                # from 10 spikes on we look for all patterns together
                if min_spikes == 10:
                    # Storing remaining parameters
                    param_dict = _storing_initial_parameters(
                        param_dict=param_dict,
                        session=session,
                        context=context,
                        job_counter=job_counter,
                        binsize=binsize,
                        unit=unit,
                        ep=ep,
                        tt=tt,
                        min_spikes=min_spikes,
                        max_spikes=None)
                    # storing min_occ
                    if min_occ <= abs_min_occ:
                        param_dict[session][context][job_counter][
                            'min_occ'] = \
                            abs_min_occ
                    else:
                        param_dict[session][context][job_counter][
                            'min_occ'] = min_occ
                        # print(f'{context} {min_spikes=} {min_occ=}')
                    param_dict = _storing_remaining_parameters(
                        param_dict=param_dict,
                        session=session,
                        context=context,
                        job_counter=job_counter,
                        percentile_poiss=percentile_poiss,
                        percentile_rates=percentile_rates,
                        winlen=winlen,
                        abs_min_spikes=abs_min_spikes)
    return param_dict


if __name__ == "__main__":
    import yaml
    from yaml import Loader

    with open("./configfile.yaml", 'r') as stream:
        config = yaml.load(stream, Loader=Loader)
    # The 5 epochs to analyze
    epochs = config['epochs']
    # The 4 trial types to analyze
    trialtypes = config['trialtypes']
    # The sessions to analyze
    sessions = config['sessions']
    # Absolute minimum number of occurrences of a pattern
    abs_min_occ = config['abs_min_occ']
    # Magnitude of the binsize used
    binsize = config['binsize']
    # The percentile for the Poisson distribution to fix minimum number of occ
    percentile_poiss = config['percentile_poiss']
    # The percentile for the Poisson distribution of rates
    percentile_rates = config['percentile_rates']
    # minimum number of spikes per patterns
    abs_min_spikes = config['abs_min_spikes']
    # The winlen parameter for the SPADE analysis
    winlen = config['winlen']
    # Unit in which every time of the analysis is expressed
    unit = config['unit']

    # loading parameters
    param_dict = estimate_number_occurrences(
        sessions=sessions,
        epochs=epochs,
        trialtypes=trialtypes,
        binsize=binsize,
        abs_min_spikes=abs_min_spikes,
        abs_min_occ=abs_min_occ,
        winlen=winlen,
        percentile_poiss=percentile_poiss,
        percentile_rates=percentile_rates,
        unit=unit)
    np.save(f'./data/{sessions[0]}/param_dict.npy', param_dict)
