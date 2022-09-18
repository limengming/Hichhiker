import m5
from m5.objects import *
 
# These three directory paths are not currently used.
gem5_dir = '/root/gem5'
spec_dir = '/root/cpu2017'
out_dir = '/root/specResult'
 
x86_suffix = '_base.mytest-m64'

#temp
binary_dir = gem5_dir+'/benches/'
data_dir = gem5_dir+'/benches/'


#500.perlbench_r
perlbench_r = Process(pid = 500) 
perlbench_r.executable = binary_dir + 'perlbench/perlbench_r_base.mytest-m64'
data = data_dir + 'perlbench/makerand.pl'
perlbench_r.cmd = [perlbench_r.executable] + ['-I.','-I./lib',data]

#600.perlbench_s
perlbench_s = Process(pid = 600)
perlbench_s.executable = binary_dir + 'perlbench/perlbench_s_base.mytest-m64'
data = data_dir + 'perlbench/makerand.pl'
perlbench_s.cmd = [perlbench_s.executable] + ['-I.','-I./lib',data]

#502.gcc_r
gcc_r = Process(pid = 502)
gcc_r.executable =  binary_dir + 'gcc/cpugcc_r_base.mytest-m64'
data = data_dir + 'gcc/t1.c'
output = data_dir + 'gcc/t1.opts-O3_-finline-limit_50000.s'
gcc_r.cmd = [gcc_r.executable] + [data] + ['-O3','-finline-limit=50000','-o',output]

#602.gcc_s Exiting @ tick 12009689000 because exiting with last active thread context
gcc_s = Process(pid = 602)
gcc_s.executable =  binary_dir+'gcc/sgcc_base.mytest-m64'
data = data_dir + 'gcc/t1.c'
output = data_dir + 'gcc/t1.opts-O3_-finline-limit_50000.s'
gcc_s.cmd = [gcc_s.executable] + [data] + ['-O3','-finline-limit=50000','-o',output]

#505.mcf_r
mcf_r = Process(pid = 505) 
mcf_r.executable = binary_dir + 'mcf/mcf_r_base.mytest-m64'
data = data_dir + 'mcf/inp.in'
mcf_r.cmd = [mcf_r.executable] + [data]

#605.mcf_s normal
mcf_s = Process(pid = 605) 
mcf_s.executable = binary_dir + 'mcf/mcf_s_base.mytest-m64'
data = data_dir + 'mcf/inp.in'
mcf_s.cmd = [mcf_s.executable] + [data]

#520.omnetpp_r error
omnetpp_r = Process(pid = 520) 
omnetpp_r.executable = binary_dir + 'omnetpp/omnetpp_r_base.mytest-m64'
# omnetpp_r.cmd = [omnetpp_r.executable]+['-c General','-r 0']
omnetpp_r.cmd = [omnetpp_r.executable]+['-c', 'General', '-r', '0']

#620.omnetpp_s 
omnetpp_s = Process(pid = 620) 
omnetpp_s.executable = binary_dir + 'omnetpp/omnetpp_s_base.mytest-m64'
# omnetpp_s.cmd = [omnetpp_s.executable]+['-c General','-r 0']
omnetpp_s.cmd = [omnetpp_s.executable]+['-c', 'General', '-r', '0']
omnetpp_s.input = binary_dir + 'omnetpp/omnetpp.ini'

#523.xalancbmk_r  seg fault when mix with s,ok when execute single s
xalancbmk_r = Process(pid = 523) 
xalancbmk_r.executable = binary_dir + 'xalancbmk/cpuxalan_r_base.mytest-m64'
data1 = data_dir + 'xalancbmk/test.xml'
data2 = data_dir + 'xalancbmk/xalanc.xsl'
xalancbmk_r.cmd = [xalancbmk_r.executable] + ['-v'] + [data1] + [data2]

#623.xalancbmk_s  
xalancbmk_s = Process(pid = 623) 
xalancbmk_s.executable = binary_dir + 'xalancbmk/xalancbmk_s_base.mytest-m64'
data1 = data_dir + 'xalancbmk/test.xml'
data2 = data_dir + 'xalancbmk/xalanc.xsl'
xalancbmk_s.cmd = [xalancbmk_s.executable] + ['-v'] + [data1] + [data2]

#525.x264_r   
x264_r = Process(pid = 525) 
x264_r.executable = binary_dir + 'x264/x264_r_base.mytest-m64'
data = data_dir + 'x264/BuckBunny.264'
# x264_r.cmd = [x264_r.executable] + ['--dumpyuv 50','--frames 156','-o', data ,'BuckBunny.yuv 1280x720' ]
x264_r.cmd = [x264_r.executable] + ['--dumpyuv', '50','--frames', '156','-o', data ,'BuckBunny.yuv', '1280x720' ]

#625.x264_s   unrecognized option '--dumpyuv 50'
x264_s = Process(pid = 625) 
x264_s.executable = binary_dir + 'x264/x264_s_base.mytest-m64'
data = data_dir + 'x264/BuckBunny.264'
# x264_s.cmd = [x264_s.executable] + ['--dumpyuv 50','--frames 156','-o', data ,'BuckBunny.yuv 1280x720' ]
x264_s.cmd = [x264_s.executable] + ['--dumpyuv', '50','--frames', '156','-o', data ,'BuckBunny.yuv', '1280x720' ]

#531.deepsjeng_r normal
deepsjeng_r = Process(pid = 531) 
deepsjeng_r.executable = binary_dir + 'deepsjeng/deepsjeng_r_base.mytest-m64'
data = data_dir + 'deepsjeng/test.txt'
deepsjeng_r.cmd = [deepsjeng_r.executable] + [data]

#631.deepsjeng_s 
deepsjeng_s = Process(pid = 631) 
deepsjeng_s.executable = binary_dir + 'deepsjeng/deepsjeng_s_base.mytest-m64'
data = data_dir + 'deepsjeng/test.txt'
deepsjeng_s.cmd = [deepsjeng_s.executable] + [data]

#541.leela_r  
leela_r = Process(pid = 541) 
leela_r.executable = binary_dir + 'leela/leela_r_base.mytest-m64'
data = data_dir + 'leela/test.sgf'
leela_r.cmd = [leela_r.executable] + [data]

#641.leela_s  normal
leela_s = Process(pid = 641) 
leela_s.executable = binary_dir + 'leela/leela_s_base.mytest-m64'
data = data_dir + 'leela/test.sgf'
leela_s.cmd = [leela_s.executable] + [data]

#548.exchange2_r   
exchange2_r = Process(pid = 548) 
exchange2_r.executable = binary_dir + 'exchange2/exchange2_r_base.mytest-m64'
exchange2_r.cmd = [exchange2_r.executable] + ['0']

#648.exchange2_s   puzzles.txt error
exchange2_s = Process(pid = 648) 
exchange2_s.executable = binary_dir + 'exchange2/exchange2_s_base.mytest-m64'
exchange2_s.cmd = [exchange2_s.executable] + ['0']
exchange2_s.input = binary_dir + 'exchange2/puzzles.txt'

#557.xz_r   
xz_r = Process(pid = 557) 
xz_r.executable = binary_dir + 'xz/xz_r_base.mytest-m64'
data = data_dir + 'xz/cpu2006docs.tar.xz'
# xz_r.cmd = [xz_r.executable] + ['cpu2006docs.tar.xz 4 055ce243071129412e9dd0b3b69a21654033a9b723d874b2015c774fac1553d9713be561ca86f74e4f16f22e664fc17a79f30caa5ad2c04fbc447549c2810fae 1548636 1555348 0 ']
xz_r.cmd = [xz_r.executable] + [data, '4', '055ce243071129412e9dd0b3b69a21654033a9b723d874b2015c774fac1553d9713be561ca86f74e4f16f22e664fc17a79f30caa5ad2c04fbc447549c2810fae', '1548636', '1555348', '0']

#657.xz_s   normal
xz_s = Process(pid = 657) 
xz_s.executable = binary_dir + 'xz/xz_s_base.mytest-m64'
# xz_s.cmd = [xz_s.executable] + ['cpu2006docs.tar.xz 4 055ce243071129412e9dd0b3b69a21654033a9b723d874b2015c774fac1553d9713be561ca86f74e4f16f22e664fc17a79f30caa5ad2c04fbc447549c2810fae 1548636 1555348 0 ']
xz_s.cmd = [xz_s.executable] + [data, '4', '055ce243071129412e9dd0b3b69a21654033a9b723d874b2015c774fac1553d9713be561ca86f74e4f16f22e664fc17a79f30caa5ad2c04fbc447549c2810fae', '1548636', '1555348', '0']

#998.specrand_is    
specrand_is = Process(pid = 998) 
specrand_is.executable = binary_dir + 'specrand_is/specrand_is_base.mytest-m64'
specrand_is.cmd = [specrand_is.executable] + ['324342', '24239']

#999.specrand_ir    
specrand_ir = Process(pid = 999) 
specrand_ir.executable = binary_dir + 'specrand_ir/specrand_ir_base.mytest-m64'
specrand_ir.cmd = [specrand_ir.executable] + ['324342', '24239']

#503.bwaves_r    normal
bwaves_r = Process(pid = 503) 
bwaves_r.executable = binary_dir + 'bwaves/bwaves_r_base.mytest-m64'
bwaves_r.cmd = [bwaves_r.executable] + ['bwaves_1']
bwaves_r.input = data_dir + 'bwaves/bwaves_1.in'

#603.bwaves_s    seg_fault
bwaves_s = Process(pid = 603) 
bwaves_s.executable = binary_dir + 'bwaves/speed_bwaves_base.mytest-m64'
bwaves_s.cmd = [bwaves_s.executable] + ['bwaves_1']
bwaves_s.input = data_dir + 'bwaves/bwaves_1.in'

#507.cactuBSSN_r   seg fault
cactuBSSN_r = Process(pid = 507) 
cactuBSSN_r.executable = binary_dir + 'cactuBSSN/cactusBSSN_r_base.mytest-m64'
data = data_dir + 'cactuBSSN/spec_test.par'
cactuBSSN_r.cmd = [cactuBSSN_r.executable] + [data]

#607.cactuBSSN_s   seg fault
cactuBSSN_s = Process(pid = 607) 
cactuBSSN_s.executable = binary_dir + 'cactuBSSN/cactuBSSN_s_base.mytest-m64'
data = data_dir + 'cactuBSSN/spec_test.par'
cactuBSSN_s.cmd = [cactuBSSN_s.executable] + [data]

#508.namd_r   
namd_r = Process(pid = 508) 
namd_r.executable = binary_dir + 'namd/namd_r_base.mytest-m64'
data = data_dir + 'namd/apoa1.test.output'
# namd_r.cmd = [namd_r.executable] + ['--input apoa1.input','--iterations 1','--output'] + [data]
namd_r.cmd = [namd_r.executable] + ['--input', data_dir + 'namd/apoa1.input','--iterations', '1','--output'] + [data]

#510.parest_r   
parest_r = Process(pid = 510) 
parest_r.executable = binary_dir + 'parest/parest_r_base.mytest-m64'
data = data_dir + 'parest/test.prm'
parest_r.cmd = [parest_r.executable] + [data]

#511.povray_r   
povray_r = Process(pid = 511) 
povray_r.executable = binary_dir + 'povray/povray_r_base.mytest-m64'
data = data_dir + 'povray/SPEC-benchmark-test.ini'
povray_r.cmd = [povray_r.executable] + [data]

#519.lbm_r   
lbm_r = Process(pid = 519) 
lbm_r.executable = binary_dir + 'lbm_r/lbm_r_base.mytest-m64'
data = data_dir + 'lbm_r/100_100_130_cf_a.of'
# lbm_r.cmd = [lbm_r.executable] + ['20 reference.dat 0 1'] + [data]
lbm_r.cmd = [lbm_r.executable] + ['20', 'reference.dat', '0', '1'] + [data]

#619.lbm_s   
lbm_s = Process(pid = 619) 
lbm_s.executable = binary_dir + 'lbm_s/lbm_s_base.mytest-m64'
data = data_dir + 'lbm_s/200_200_260_ldc.of'
# lbm_s.cmd = [lbm_s.executable] + ['20 reference.dat 0 1'] + [data]
lbm_s.cmd = [lbm_s.executable] + ['20', 'reference.dat', '0', '1'] + [data]

#521.wrf_r     
wrf_r = Process(pid = 521) 
wrf_r.executable = binary_dir + 'wrf/wrf_r_base.mytest-m64'
wrf_r.cmd = [wrf_r.executable]

#621.wrf_s     
wrf_s = Process(pid = 621) 
wrf_s.executable = binary_dir + 'wrf/wrf_s_base.mytest-m64'
wrf_s.cmd = [wrf_s.executable]
wrf_s.input = binary_dir + 'wrf/namelist.input'

#526.blender_r   
blender_r = Process(pid = 526) 
blender_r.executable = binary_dir + 'blender/blender_r_base.mytest-m64'
data = data_dir + 'blender/cube.blend'
# blender_r.cmd = [blender_r.executable] + [data] + ['--render-output cube_','--threads 1','-b -F RAWTGA -s 1 -e 1 -a']
blender_r.cmd = [blender_r.executable] + [data] + ['--render-output', 'cube_','--threads', '1','-b', '-F', 'RAWTGA', '-s', '1', '-e', '1', '-a']

#527.cam4_r      
cam4_r = Process(pid = 527) 
cam4_r.executable = binary_dir + 'cam4/cam4_r_base.mytest-m64'
cam4_r.cmd = [cam4_r.executable]

#627.cam4_s      
cam4_s = Process(pid = 627) 
cam4_s.executable = binary_dir + 'cam4/cam4_s_base.mytest-m64'
cam4_s.cmd = [cam4_s.executable]

#628.pop2_s      
pop2_s = Process(pid = 628) 
pop2_s.executable = binary_dir + 'pop2/speed_pop2_base.mytest-m64'
pop2_s.cmd = [pop2_s.executable]

#538.imagick_r    
imagick_r = Process(pid = 538) 
imagick_r.executable = binary_dir + 'imagick/imagick_r_base.mytest-m64'
data = data_dir + 'imagick/test_input.tga'
# imagick_r.cmd = [imagick_r.executable] + ['-limit disk 0 '] + [data] + [' -shear 25 -resize 640x480 -negate -alpha Off test_output.tga ']
imagick_r.cmd = [imagick_r.executable] + ['-limit', 'disk', '0'] + [data] + ['-shear', '25', '-resize', '640x480', '-negate', '-alpha', 'Off', 'test_output.tga']

#638.imagick_s    
imagick_s = Process(pid = 638) 
imagick_s.executable = binary_dir + 'imagick/imagick_s_base.mytest-m64'
data = data_dir + 'imagick/test_input.tga'
# imagick_s.cmd = [imagick_s.executable] + ['-limit disk 0'] + [data] + [' -shear 25 -resize 640x480 -negate -alpha Off test_output.tga ']
imagick_s.cmd = [imagick_s.executable] + ['-limit', 'disk', '0'] + [data] + ['-shear', '25', '-resize', '640x480', '-negate', '-alpha', 'Off', 'test_output.tga']

#544.nab_r     
nab_r = Process(pid = 544) 
nab_r.executable = binary_dir + 'nab/nab_r_base.mytest-m64'
# data = data_dir + 'nab/hkrdenq/hkrdenq'
data = 'hkrdenq'
# nab_r.cmd = [nab_r.executable] + [data] + ['1930344093 1000']
nab_r.cmd = [nab_r.executable] + [data] + ['1930344093', '1000']

#644.nab_s     
nab_s = Process(pid = 644) 
nab_s.executable = binary_dir + 'nab/nab_s_base.mytest-m64'
# data = data_dir + 'nab/hkrdenq'
data = 'hkrdenq'
# nab_s.cmd = [nab_s.executable] + [data] + ['1930344093 1000']
nab_s.cmd = [nab_s.executable] + [data] + ['1930344093', '1000']

#549.fotonik3d_r       
fotonik3d_r = Process(pid = 549) 
fotonik3d_r.executable = binary_dir + 'fotonik3d/fotonik3d_r_base.mytest-m64'
fotonik3d_r.cmd = [fotonik3d_r.executable]

#649.fotonik3d_s       
fotonik3d_s = Process(pid = 649) 
fotonik3d_s.executable = binary_dir + 'fotonik3d/fotonik3d_s_base.mytest-m64'
fotonik3d_s.cmd = [fotonik3d_s.executable]

#554.roms_r     
roms_r = Process(pid = 554) 
roms_r.executable = binary_dir + 'roms/roms_r_base.mytest-m64'
roms_r.cmd = [roms_r.executable]
roms_r.input = data_dir + 'roms/ocean_benchmark0.in.x'

#654.roms_s     
roms_s = Process(pid = 654) 
roms_s.executable = binary_dir + 'roms/sroms_base.mytest-m64'
roms_s.cmd = [roms_s.executable]
roms_s.input = data_dir + 'roms/ocean_benchmark0.in.x'

#996.specrand_fs      
specrand_fs = Process(pid = 996) 
specrand_fs.executable = binary_dir + 'specrand_fs/specrand_fs_base.mytest-m64'
# specrand_fs.cmd = [specrand_fs.executable] + ['324342 24239']
specrand_fs.cmd = [specrand_fs.executable] + ['324342', '24239']

#997.specrand_fr       
specrand_fr = Process(pid = 997) 
specrand_fr.executable = binary_dir + 'specrand_fr/specrand_fr_base.mytest-m64'
# specrand_fr.cmd = [specrand_fr.executable] + ['324342 24239']
specrand_fr.cmd = [specrand_fr.executable] + ['324342', '24239']
