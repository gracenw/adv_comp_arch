import numpy as np

ipc_log = []
with open('search_waves.out', 'rt') as f:
    data = f.readlines()
for line in data:
    if 'IPC:' in line:
        line = line.replace(" ", "")
        line = line.split(':')[1][:-1]
        ipc_log.append(float(line))

ipc_log = np.array(ipc_log)
print(np.amax(ipc_log))
