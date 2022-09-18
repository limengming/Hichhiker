import os

for bench in ['gcc_r', 'imagick_r', 'leela_r', 'omnetpp_r', 'parest_r', 'xalancbmk_r']:
    os.system("./gem5.opt -d %s --debug-flags=InstProgress --debug-file=progHC.out --stats-file=hitchC.txt run3_hr.py -b %s -I 100000000" % (bench, bench))
    os.system("./gem5.opt -d %s --debug-flags=InstProgress --debug-file=progHD.out --stats-file=hitchD.txt run3_h.py -b %s -I 100000000" % (bench, bench))
    # os.system("./gem5.opt -d %s --debug-flags=InstProgress --debug-file=progS.out --stats-file=shadow.txt run3_s.py -b %s -I 100000000" % (bench, bench))