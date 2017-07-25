import sys
from plot_scripts.parser import Parser

parser = Parser(sys.argv[1], {"time":0, "size":1, "src":2, "dst":3}, delimiter= " ")


time =5

hosts_to_flows = {}

for flow in parser:

    now = float(flow[parser.get_index("time")])
    size = int(flow[parser.get_index("size")])
    src = flow[parser.get_index("src")]
    dst = flow[parser.get_index("dst")]
    
    if now > time :
        continue


    if src not in hosts_to_flows:
        hosts_to_flows[src] = {"tx": size, "rx":0}

    else:
        hosts_to_flows[src]["tx"] +=size


    if dst not in hosts_to_flows:
        hosts_to_flows[dst] = {"tx": 0, "rx":size}

    else:
        hosts_to_flows[dst]["rx"] +=size


for host, tx_rx in hosts_to_flows.iteritems():
    print host, " tx: ", tx_rx["tx"], " rx: ", tx_rx["rx"]
