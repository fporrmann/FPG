import elephant.spade as spade
import elephant.conversion as conv


def generate_input(data, binsize, winlen):
    """
    Generates input for FIM.fpgrowth algorithm used in spade.
    First discretizes data and then uses spade._build_context function

    Parameters
    ----------
    data : list
        list of neo.Spiketrains to analyze with SPADE
    binsize : pq.quantity
        width of the discretization in milliseconds
    winlen : int
        length of the moving window for pattern detection

    Returns
    -------
    transactions : : list
        List of all transactions, each element of the list contains the
        attributes of the corresponding object.
    """
    # create fim input as list of transactions
    binary_matrix = conv.BinnedSpikeTrain(
             data, binsize, tolerance=None).to_sparse_bool_array().tocoo()

    context, transactions, rel_matrix = \
        spade._build_context(binary_matrix=binary_matrix,
                             winlen=winlen)
    return transactions


if __name__ == '__main__':
    import numpy as np
    import quantities as pq
    import argparse
    import yaml
    from yaml import Loader

    # Load general parameters
    with open("configfile.yaml", 'r') as stream:
        config = yaml.load(stream, Loader=Loader)

    # SPADE parameters
    winlen = config['winlen']
    binsize = config['binsize'] * pq.ms
    dataset = config['sessions'][0]
    epoch = config['epochs'][0]
    trialtype = config['trialtypes'][0]

    # Loading data
    sts = np.load(f'./data/{dataset}/spiketrains_{epoch}_{trialtype}.npy',
                  allow_pickle=True)
    sts = list(sts)

    # Generate input for fim
    fim_input = generate_input(data=sts,
                               binsize=binsize,
                               winlen=winlen)
    # TODO: fix numpy deprecation warning
    np.save(f'./data/{dataset}/{epoch}_{trialtype}.npy', fim_input)
    # TODO: step to get to the txt file is missing (ask FP)
