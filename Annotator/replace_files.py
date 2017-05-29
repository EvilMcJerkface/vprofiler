from pdb import set_trace
import subprocess

output = subprocess.check_output('find /home/jiamin/httpd -name "*vprof*"', shell=True)

for fname in output.split('\n'):
    if fname != '':
        vprofFname = fname.rstrip('\n')
        origFname = vprofFname.replace('_vprof', '')
        subprocess.Popen(['cp', vprofFname, origFname])
