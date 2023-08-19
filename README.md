# CLOSER : Constraint Learning fOr StrEam Reasoning
## Overview
A DRL framework to effectively manage learned constraints in stream reasoning engines. This framework seamlessly integrates with ASP solvers, making it compatible with ASP-based stream reasoning systems.
## Usage
```bash
conda create -n closer python=3.7 pytorch=1.1.0
conda activate closer
python -m pip install --upgrade pip
python -m pip install pyflann
sudo 2to3 -w /home/mourad/miniconda3/envs/closer/lib/python3.7/site-packages/pyflann
python -m pip install gym==0.9.6
python -m pip install setproctitle
```
Run the three commands in three separate terminals
```bash
.Reasoner/build/release/wasp --executeRL=true < nqueens-instances/stream/enc-30
```
```bash
cat Reasoner/nqueens-instances/stream/log-30 > reconfigurate
```
```bash
python wolpertinger/main.py
```
