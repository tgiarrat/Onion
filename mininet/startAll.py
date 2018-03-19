from mininet.cli import CLI
from mininet.log import lg, info
from mininet.topolib import TreeNet

net = TreeNet(depth=1,fanout=7)

net.start()
net.hosts[6].cmd('cd http')
net.hosts[6].cmd('python server.py &')
for h in net.hosts:
    if h.name != 'h7':
        h.cmd('cd '+h.name);
        h.cmd('../Onion 10.0.0.7 &')

CLI(net);

net.stop()
