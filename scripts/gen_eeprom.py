import os
import sys

prefix = os.environ.get('PREFIX', '')

for bin_file in sys.argv[1:]:
    var_name = prefix + bin_file.split('.')[0]
    sys.stdout.write('static const uint8_t %s[] = {' % var_name)
    with open(bin_file, 'rb') as f:
        s = f.read()
        for i, b in enumerate(s):
            if i % 16 == 0:
                sys.stdout.write('\n    ')
            elif i != 0:
                sys.stdout.write(' ')
            sys.stdout.write('0x%02x,' % b)
    sys.stdout.write('\n};\n')
