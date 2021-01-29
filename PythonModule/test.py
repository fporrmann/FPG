import fim as m
import numpy as np

# a = [[ord('f'), ord('g'), ord('a'), ord('b'), ord('c')], [ord('f'), ord('g'), ord('a'), ord('b'), ord('c')], [ord('f'), ord('g')], [ord('f')], [ord('g'), ord('a'), ord('c')], [ord('f'), ord('z'), ord('g'), ord('a')], [ord('f'), ord('z'), ord('g'), ord('a')], [ord('f'), ord('z')], [ord('f')], [ord('f'), ord('a')], [ord('f')]]

filename = 'fim_input.txt'
# filename = 'transactions_cpp.txt'
transactions = []

for line in open(filename):
	transactions.append([int(i) for i in line.split()])

res = m.fpgrowth(tracts=transactions,target='c',supp=88,zmin=2,zmax=0,report='a',algo='s',verbose=1)
res = m.fpgrowth(tracts=transactions,target='c',supp=25,zmin=3,zmax=0,report='a',algo='s',verbose=1)
res = m.fpgrowth(tracts=transactions,target='c',supp=12,zmin=4,zmax=0,report='a',algo='s',verbose=1)
res = m.fpgrowth(tracts=transactions,target='c',supp=10,zmin=5,zmax=0,report='a',algo='s',verbose=1)
res = m.fpgrowth(tracts=transactions,target='c',supp=10,zmin=6,zmax=0,report='a',algo='s',verbose=1)
res = m.fpgrowth(tracts=transactions,target='c',supp=10,zmin=7,zmax=0,report='a',algo='s',verbose=1)
res = m.fpgrowth(tracts=transactions,target='c',supp=10,zmin=8,zmax=0,report='a',algo='s',verbose=1)
res = m.fpgrowth(tracts=transactions,target='c',supp=10,zmin=9,zmax=0,report='a',algo='s',verbose=1)

#np.save('results_np.npy', res)

#f = open('results.txt', 'w')
#for fp in res:
#	l = list(fp[0])
#	l.sort()
#	for neuron in l:
#		f.write(str(neuron) + ' ')
#	f.write('Occurrences: ' + str(fp[1]) + '\n')

#f.close()
