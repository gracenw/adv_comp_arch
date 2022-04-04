ipc_log = []
with open('search_gcc.out', 'rt') as f:
    data = f.readlines()
for line in data:
    if 'IPC:' in line:
        line = line.replace(" ", "")
        line = line.split(':')[1]
        ipc_log.append(int(line))
print(ipc_log)