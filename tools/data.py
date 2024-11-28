import gzip
import os
import sys

os.makedirs('.pio/data', exist_ok=True)
for filename in ['logo.png', 'apple-touch-icon.png', 'favicon-96x96.png', 'favicon.svg']:
    skip = False
    if os.path.isfile('.pio/data/' + filename + '.timestamp'):
        with open('.pio/data/' + filename + '.timestamp', 'r', -1, 'utf-8') as timestampFile:
            if os.path.getmtime('data/' + filename) == float(timestampFile.readline()):
                skip = True
    if skip:
        sys.stderr.write(f"data.py: {filename} up to date\n")
        continue
    with open('data/' + filename, 'rb') as inputFile:
        with gzip.open('.pio/data/' + filename + '.gz', 'wb') as outputFile:
            sys.stderr.write(f"data.py: gzip \'data/' + {filename} + '\' to \'.pio/data/' + {filename} + '.gz\'\n")
            outputFile.writelines(inputFile)
    with open('.pio/data/' + filename + '.timestamp', 'w', -1, 'utf-8') as timestampFile:
        timestampFile.write(str(os.path.getmtime('data/' + filename)))