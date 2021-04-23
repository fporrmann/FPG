import fim
import json
import numpy as np
import sys

if len(sys.argv) < 2:
	print("Usage: {} <CONFIG>".format(sys.argv[0]))
	sys.exit()

transactions = []
threads=0 # Use max. number of possible threads
verbose=1
cfgFile=sys.argv[1]

with open(cfgFile, 'r') as file:
    cfg = json.load(file)

# Read the transaction database
for line in open("datasets/{}".format(cfg['filename'])):
	transactions.append([int(i) for i in line.split()])


for job in cfg['jobs']:
	# The result 'res' is a numpy array
	res = fim.fpgrowth(tracts=transactions, target='c', supp=job['min_supp'], zmin=job['min_occ'], zmax=0, report='a', algo='s', min_neu=job['min_neu'], verbose=verbose, winlen=cfg['winlen'], threads=threads)
