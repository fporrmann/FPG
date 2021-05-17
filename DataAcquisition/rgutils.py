import os
import sys
import numpy as np
import quantities as pq
sys.path.insert(0, './multielectrode_grasp/code/python-neo')
import neo
sys.path.insert(0, './multielectrode_grasp/code/python-odml')
sys.path.insert(0, './multielectrode_grasp/code/reachgraspio')
import reachgraspio as rgio
sys.path.insert(0, './multielectrode_grasp/code')
from neo_utils import add_epoch, cut_segment_by_epoch, get_events
from elephant import spike_train_synchrony


def data_path(session):
    """
    Finds the path associated to a given session.

    Parameters
    ----------
    session : str or ReachGraspIO object
        if a string, the name of a recording subsession. E.g: 'l101126-002'
        Otherwise, a rg.rgio.ReachGraspIO object.

    Returns
    -------
    path : str
        the path of the given session

    """
    if type(session) == str:
        path = os.path.dirname(os.getcwd()) + \
               '/DataAcquisition/multielectrode_grasp/datasets/'
    elif type(session) == rgio.ReachGraspIO:
        fullpath = session.filename
        path = ''
        for s in fullpath.split('/')[:-1]:
            path = path + s + '/'
    path = os.path.abspath(path) + '/'
    return path


def odml_path(session):
    """
    Finds the path of the odml file associated to a given session.

    Parameters
    ----------
    session : str or ReachGraspIO object
        if a string, the name of a recording subsession. E.g: 'l101126-002'
        Otherwise, a rg.rgio.ReachGraspIO object.

    Returns
    -------
    path : str
        the path of the given session odml

    """
    if type(session) == str:
        path = os.path.dirname(os.getcwd()) + \
               '/DataAcquisition/multielectrode_grasp/datasets/'
    return path


def _session(session_name):
    """
    Wrapper to load a ReachGraspIO session if input is a str, and do nothing
    if input is already a ReachGraspIO object.
    Returns the session and its associated filename.

    Parameters:
    -----------
    session : str of session loaded with ReachGraspIO
        if a string, the name of a recording subsession. E.g: 'l101126-002'
        Otherwise, a rg.rgio.ReachGraspIO object.

    Returns
    -------
    session : ReachGraspIO
    session_name : str
    """
    path = data_path(session_name)
    path_odml = odml_path(session_name)
    session = rgio.ReachGraspIO(path + session_name, odml_directory=path_odml)
    return session


def st_id(spiketrain):
    """
    associates to a Lilou's SpikeTrain st an unique ID, given by the float
    100* electrode_id + unit_id.
    E.g.: electrode_id = 7, unit_id = 1 -> st_id = 701
    """
    return spiketrain.annotations['channel_id'] * 100 + 1 * \
        spiketrain.annotations['unit_id']


def shift_spiketrain(spiketrain, t):
    """
    Shift the times of a SpikeTrain by an amount t.
    Shifts also t_start and t_stop by t.
    Retains the spike train's annotations, waveforms, sampling rate.
    """
    st = spiketrain
    st_shifted = neo.SpikeTrain(
        st.view(pq.Quantity) + t, t_start=st.t_start + t,
        t_stop=st.t_stop + t, waveforms=st.waveforms)
    st_shifted.sampling_period = st.sampling_period
    st_shifted.annotations = st.annotations

    return st_shifted


def SNR_kelly(spiketrain):
    """
    returns the SNR of the waveforms of spiketrains, as computed in
    Kelly et al (2007):
    * compute the mean waveform
    * define the signal as the peak-to-through of such mean waveform
    * define the noise as double the std.dev. of the values of all waveforms,
      each normalised by subtracting the mean waveform

    Parameters:
    -----------
    spiketrain : SpikeTrain
        spike train loaded with rgio (has attribute "vaweforms")

    Returns:
    --------
    snr: float
        The SNR of the input spike train
    """
    mean_waveform = spiketrain.waveforms.mean(axis=0)
    signal = mean_waveform.max() - mean_waveform.min()
    SD = (spiketrain.waveforms - mean_waveform).std()
    return signal / (2. * SD)


def calc_spiketrains_SNR(session, units='all'):
    """
    Calculates the signal-to-noise ratio (SNR) of each SpikeTrain in the
    specified session.

    Parameters:
    -----------
    session : str of session loaded with ReachGraspIO
        if a string, the name of a recording subsession. E.g: 'l101126-002'
        Otherwise, a rg.rgio.ReachGraspIO object.
    units : str
        which type of units to consider:
        * 'all': returns the SNR values of all units in the session
        * 'sua': returns SUAs' SNR values only
        * 'mua': returns MIAs' SNR values only

    Returns:
    --------
    SNRdict : dict
        a dictionary of unit ids and associated SNR values
    """
    session = _session(session)

    block = session.read_block(channel_list=list(range(1, 97)), nsx=[2], units=[],
        waveforms=True)

    sts = [st for st in block.segments[0].spiketrains]
    if units == 'sua':
        sts = [st for st in sts if st.annotations['sua']]
    elif units == 'mua':
        sts = [st for st in sts if st.annotations['mua']]

    SNRdict = {}
    for st in sts:
        sua_id = st_id(st)
        SNRdict[sua_id] = SNR_kelly(st)

    return SNRdict


# ==========================================================================
# Loading routines:
# load_session(): loads spike trains from a full session
# load_epoch_as_list(): load spike trains from an epoch; for each SUA, a list
# load_epoch_concatenated_trials(): load concat'd spike trains from an epoch
# ==========================================================================


def load_epoch_as_lists(session_name, epoch, trialtypes=None, SNRthresh=0,
                        verbose=False):
    """
    Load SUA spike trains of specific session and epoch from Lilou's data.

    * The output is a dictionary, with SUA ids as keys.
    * Each SUA id is associated to a list of spike trains, one per trial.
    * Each SpikeTrain is aligned to the epoch's trigger (see below) and has
      annotations indicating the corresponding trial type and much more.

    The epoch is either one of 6 specific epochs defined, following a
    discussion with Sonja, Alexa, Thomas, as 500 ms-long time segments
    each around a specific trigger, or a triplet consisting of a trigger
    and two time spans delimiting a time segment around the trigger.
    Additionally allows to select only one or more trialtypes, to consider
    SUAs with a minimum waveforms' signal-to-noise ratio, to remove
    synchronous spiking events of a certain minimum size (defined at a given
    time scale).
    The spike trains can be centered at the trigger associated to the epoch,
    or at the left or right end of the corresponding time segment.

    The pre-defined epochs, the associated triggers, and the time spans
    t_pre and t_post (before and after the trigger, respectively) are:
    * epoch='start'     :  trigger='FP-ON'   t_pre=250 ms   t_post=250 ms
    * epoch='cue1'      :  trigger='CUE-ON'  t_pre=250 ms   t_post=250 ms
    * epoch='earlydelay':  trigger='FP-ON'   t_pre=0 ms     t_post=500 ms
    * epoch='latedelay' :  trigger='GO-ON'   t_pre=500 ms   t_post=0 ms
    * epoch='movement'  :  trigger='SR'      t_pre=200 ms   t_post=300 ms
    * epoch='hold'      :  trigger='RW'      t_pre=500 ms   t_post=0 ms

    Parameters:
    -----------
    session : str of session loaded with ReachGraspIO
        if a string, the name of a recording subsession. E.g: 'l101126-002'
        Otherwise, a rg.rgio.ReachGraspIO object.
    epoch : str or triplet
        if str, defines a trigger and a time segment around it (see above).
        if a triplet (tuple with 3 elements), its elements are, in order:
        * trigger [str] : a trigger (any string in session.trial_events)
        * t_pre [Quantity] : the left end of the time segment around the
          trigger. (> 0 for times before the trigger, < 0 for time after it)
        * t_post [Quantity] : the right end of the time segment around the
          trigger. (> 0 for times after the trigger, < 0 for time before it)
    trialtypes : str
        One  trial type, among those present in the session.
        8 Classical trial types for Lilou's sessions are:
        'SGHF', 'SGLF', 'PGHF', PGLF', 'HFSG', 'LFSG', 'HFPG', 'LFPG'.
        trialtypes can be one of such strings, or None.
        If None, all trial types in the session are considered.
        Default: None
    SNRthresh : float, optional
        lower threshold for the waveforms' SNR of SUAs to be considered.
        SUAs with a lower or equal SNR are not loaded.
        Default: 0
    dt : Quantity, optional
        time lag within which synchronous spikes are considered highly
        synchronous ("synchrofacts"). If None, the sampling period of the
        recording system (1 * session.nev_unit) is used.
        Default: None
    dt2 : Quantity, optional
        isolated spikes falling within a time lag dt2 from synchrofacts (see
        parameter dt) to be removed (see parameter synchsize) are also
        removed. If None, the sampling period of the recording system
        (1 * session.nev_unit) is used.
        Default: None
    verbose : bool, optional
        Whether to print information as different steps are run

    Returns:
    --------
    data : dict
        a dictionary having SUA IDs as keys (see st_id) and lists of
        SpikeTrains as corresponding values.
        Each SpikeTrain corresponds to the SUA spikes in one trial (having
        the specified trial type(s)), during the specified epoch. It retains
        the annotations (e.g. trial, electrode and unit id) of the original
        data. Additionally, it has the keys 'trial_type', 'epoch', 'trigger',
        't_pre' and 't_post', as specified in input
    """
    # Define trigger, t_pre, t_post depending on session_name
    if epoch == 'start':
        trigger, t_pre, t_post = 'TS-ON', -250 * pq.ms, 250 * pq.ms
    elif epoch == 'cue1':
        trigger, t_pre, t_post = 'CUE-ON', -250 * pq.ms, 250 * pq.ms
    elif epoch == 'earlydelay':
        trigger, t_pre, t_post = 'CUE-OFF', -0 * pq.ms, 500 * pq.ms
    elif epoch == 'latedelay':
        trigger, t_pre, t_post = 'GO-ON', -500 * pq.ms, 0 * pq.ms
    elif epoch == 'movement':
        trigger, t_pre, t_post = 'SR', -200 * pq.ms, 300 * pq.ms
    elif epoch == 'hold':
        trigger, t_pre, t_post = 'RW-ON', -500 * pq.ms, 0 * pq.ms
    elif isinstance(epoch, str):
        raise ValueError("epoch '%s' not defined" % epoch)
    elif len(epoch) == 3:
        trigger, t_pre, t_post = epoch
    else:
        raise ValueError('epoch must be either a string or a tuple of len 3')

    # Load session, and create block depending on the trigger
    session = _session(session_name)
    if verbose:
        print(('Load data (session: %s, epoch: %s, trialtype: %s)...' % (
            session_name, epoch, trialtypes)))
        print("  > load session %s, and define Block around trigger '%s'..." %
              (session_name, trigger))

    block = session.read_block(
        nsx_to_load=None,
        n_starts=None,
        n_stops=None,
        channels=list(range(1, 97)),
        units='all',
        load_events=True,
        load_waveforms=False,
        scaling='raw')

    data_segment = block.segments[0]
    start_events = get_events(
        data_segment,
        properties={
            'trial_event_labels': trigger,
            'performance_in_trial': session.performance_codes['correct_trial']})
    start_event = start_events[0]
    epoch = add_epoch(
        data_segment,
        event1=start_event, event2=None,
        pre=t_pre, post=t_post,
        attach_result=False,
        name='{}'.format(epoch))
    cut_trial_block = neo.Block(name="Cut_Trials")
    cut_trial_block.segments = cut_segment_by_epoch(
        data_segment, epoch, reset_time=True)
    selected_trial_segments = cut_trial_block.filter(
        targdict={'belongs_to_trialtype': trialtypes}, objects=neo.Segment)
    data = {}
    for seg_id, seg in enumerate(selected_trial_segments):
        for st in seg.filter({'sua': True}):
            # Check the SNR
            if st.annotations['SNR'] > SNRthresh:
                st.annotations['trial_id'] = seg.annotations[
                    'trial_id']
                st.annotations['trial_type'] = seg.annotations[
                    'belongs_to_trialtype']
                st.annotate(trial_id_trialtype=seg_id)
                el = st.annotations['channel_id']
                sua = st.annotations['unit_id']
                sua_id = el * 100 + sua * 1
                try:
                    data[sua_id].append(st)
                except:
                    data[sua_id] = [st]
    return data


def load_epoch_concatenated_trials(
    session, epoch, trialtypes=None, SNRthresh=0, synchsize=0, dt=1,
        sep=100*pq.ms, verbose=False):
    """
    Load a slice of Lilou's spike train data in a specified epoch
    (corresponding to a trigger and a time segment aroun it), select spike
    trains corresponding to specific trialtypes only, and concatenate them.

    The epoch is either one of 6 specific epochs defined, following a
    discussion with Sonja, Alexa, Thomas, as 500 ms-long time segments
    each around a specific trigger, or a triplet consisting of a trigger
    and two time spans delimiting a time segment around the trigger.

    The pre-defined epochs, the associated triggers, and the time spans
    t_pre and t_post (before and after the trigger, respectively) are:
    * epoch='start'     :  trigger='FP-ON'   t_pre=250 ms   t_post=250 ms
    * epoch='cue1'      :  trigger='CUE-ON'  t_pre=250 ms   t_post=250 ms
    * epoch='earlydelay':  trigger='FP-ON'   t_pre=0 ms     t_post=500 ms
    * epoch='latedelay' :  trigger='GO-ON'   t_pre=500 ms   t_post=0 ms
    * epoch='movement'  :  trigger='SR'      t_pre=200 ms   t_post=300 ms
    * epoch='hold'      :  trigger='RW'      t_pre=500 ms   t_post=0 ms

    Parameters:
    -----------
    session : str of session loaded with ReachGraspIO
        if a string, the name of a recording subsession. E.g: 'l101126-002'
        Otherwise, a rg.rgio.ReachGraspIO object.
    epoch : str or triplet
        if str, defines a trigger and a time segment around it (see above).
        if a triplet (tuple with 3 elements), its elements are, in order:
        * trigger [str] : a trigger (any string in session.trial_events)
        * t_pre [Quantity] : the left end of the time segment around the
          trigger. (> 0 for times before the trigger, < 0 for time after it)
        * t_post [Quantity] : the right end of the time segment around the
          trigger. (> 0 for times after the trigger, < 0 for time before it)
    trialtypes : str | list of str | None, optional
        One or more trial types, among those present in the session.
        8 Classical trial types for Lilou's sessions are:
        'SGHF', 'SGLF', 'PGHF', PGLF', 'HFSG', 'LFSG', 'HFPG', 'LFPG'.
        trialtypes can be one of such strings, of a list of them, or None.
        If None, all trial types in the session are considered.
        Default: None
    SNRthresh : float, optional
        lower threshold for the waveforms' SNR of SUAs to be considered.
        SUAs with a lower or equal SNR are not loaded.
        Default: 0
    synchsize : int, optional
        minimum size of synchronous events to be removed from the data.
        If 0, no synchronous events are removed.
        Synchrony is defined by the parameter dt.
    dt : Quantity, optional
        time lag within which synchronous spikes are considered highly
        synchronous ("synchrofacts"). If 1, the sampling period of the
        recording system (1 * session.nev_unit) is used.
        Default: 1
    sep : Quantity
        Time interval used to separate consecutive trials.
    verbose : bool
        Whether to print information as different steps are run
    firing_rate_threshold: None or float
        Threshold for excluding neurons with high firing rate
        Default: None

    Returns:
    --------
    data : list
        a list of SpikeTrains, each obtained by concatenating all trials of the desired
        type(s) and during the specified epoch for that SUA.
    """
    # Load the data as a dictionary of SUA_id: [list of trials]
    data = load_epoch_as_lists(session, epoch, trialtypes=trialtypes,
                               SNRthresh=SNRthresh,
                               verbose=verbose)

    # Check that all spike trains in all lists have same t_start, t_stop
    t_pre = abs(list(data.values())[0][0].t_start)
    t_post = abs(list(data.values())[0][0].t_stop)
    if not all([np.all([abs(st.t_start) == t_pre
        for st in st_list]) for st_list in list(data.values())]):
            raise ValueError(
                'SpikeTrains have not same t_pre; cannot be concatenated')
    if not all([np.all([abs(st.t_stop) == t_post
        for st in st_list]) for st_list in list(data.values())]):
            raise ValueError(
                'SpikeTrains have not same t_post; cannot be concatenated')

    # Define time unit (nev_unit), trial duration, trial IDs to consider
    time_unit = list(data.values())[0][0].units
    trial_duration = (t_post + t_pre + sep).rescale(time_unit)
    trial_ids_of_chosen_types = np.unique(np.hstack([
        [st.annotations['trial_id_trialtype'] for st in st_list]
        for st_list in list(data.values())]))

    # Concatenate the lists of spike trains into a single SpikeTrain
    if verbose:
        print('  > concatenate trials...')
    conc_data = []
    for sua_id in sorted(data.keys()):
        trials_to_concatenate = []
        original_times = []
        # Create list of trials, each shifted by trial_duration*trial_id
        for tr in data[sua_id]:
            trials_to_concatenate.append(
                tr.rescale(time_unit).magnitude + (
                    (trial_duration * tr.annotations[
                        'trial_id_trialtype']).rescale(time_unit)).magnitude)
            original_times.extend(list(tr.magnitude))
        # Concatenate the trials (time unit lost!)
        if len(trials_to_concatenate) > 0:
            trials_to_concatenate = np.hstack(trials_to_concatenate)

        # Re-transform the concatenated spikes into a SpikeTrain
        st = neo.SpikeTrain(trials_to_concatenate * time_unit,
            t_stop=trial_duration * max(trial_ids_of_chosen_types) +
                   trial_duration).rescale(pq.s)

        # Copy into the SpikeTrain the original annotations
        for key, value in list(data[sua_id][0].annotations.items()):
            if key != 'trial_id':
                st.annotations[key] = value
        st.annotate(original_times=original_times)
        conc_data.append(st)
    # Remove exactly synchronous spikes from data
    if not (synchsize == 0 or synchsize is None):
        sampling_rate = 30000 * pq.Hz
        obj = spike_train_synchrony.Synchrotool(conc_data,
                                                sampling_rate=sampling_rate,
                                                spread=dt,
                                                tolerance=None)
        obj.delete_synchrofacts(threshold=synchsize, in_place=True)
        sts = obj.input_spiketrains
        for i in range(len(conc_data)):
            sts[i].annotations = conc_data[i].annotations
    else:
        sts = conc_data
    # Return the list of SpikeTrains
    return sts
