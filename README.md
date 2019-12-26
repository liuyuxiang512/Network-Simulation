# Network-Simulation
Simulate a network environment and design a routing mechanism with **NS-3**.

## Files
* *adjacency_matrix.txt* contains the network topology.
* *hw_all.cc* will output all simulation results with many intermediate results.
* *hw_clear.cc* only outputs 4 indexes (max queue length, average time delay, average throughput, and average packet loss rate) into 4 *.dat* files.

## How to run
Installing NS-3 is required.
```
./waf
```
```
./waf -run scratch/hw_clear
```
