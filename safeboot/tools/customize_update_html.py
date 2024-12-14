import gzip
import os
import sys
import re
from datetime import datetime

Import("env")

def readFlag(flag):
    buildFlags = env.ParseFlags(env["BUILD_FLAGS"])
    # print(buildFlags.get("CPPDEFINES"))
    for define in buildFlags.get("CPPDEFINES"):
        if (define == flag or (isinstance(define, list) and define[0] == flag)):
            # print("Found "+flag+" = "+define[1])
            # strip quotes ("") from define[1]
            cleanedFlag = re.sub(r'^"|"$', '', define[1])
            return cleanedFlag
    return None

os.makedirs('.pio/data', exist_ok=True)
for filename in ['update.html']:
    # always rebuild to update the build datetime string
    # skip = False
    # if os.path.isfile('.pio/data/' + filename + '.timestamp'):
    #     with open('.pio/data/' + filename + '.timestamp', 'r', -1, 'utf-8') as timestampFile:
    #         if os.path.getmtime('data/' + filename) == float(timestampFile.readline()):
    #             skip = True
    # if skip:
    #     sys.stderr.write(f"customize_update_html.py: {filename} up to date\n")
    #     continue
    with open('data/' + filename, 'r') as inputFile:
        for count, line in enumerate(inputFile):
            pass
        sys.stderr.write(f"customize_update_html.py: Total Lines: {count + 1}\n")
        board = env.GetProjectOption('board')
        sys.stderr.write(f"customize_update_html.py: board: {board}\n")
        app_name = readFlag("APP_NAME")
        sys.stderr.write(f"customize_update_html.py: app_name: {app_name}\n")
        timestamp = datetime.now().isoformat(sep=' ', timespec='seconds')
        sys.stderr.write(f"customize_update_html.py: timestamp: {timestamp}\n")

    with open('data/' + filename, 'r') as inputFile:
        lines = "<!-- DO NOT EDIT - Created by customize_update_html.py -->\n"
        with open('data/' + "customized_" + filename, 'w') as outputFile:
            for _ in range(count):
                line = inputFile.readline()
                if "<title>" in line:
                    lines += f"    <title>{app_name}</title>\n"
                elif "<h1 id=\"title\">" in line:
                    lines += f"      <h1 id=\"title\">{app_name} Firmware Update</h1>\n"
                elif "<script>" in line:
                    lines += line
                    lines += f"      const info_text_board_value = \"{board}\"\n"
                    lines += f"      const info_text_build_value = \"{timestamp}\"\n"
                else:
                    lines += line

            outputFile.write(lines)

    with open('data/' + "customized_" + filename, 'rb') as inputFile:
        with gzip.open('.pio/data/' + filename + '.gz', 'wb') as outputFile:
            sys.stderr.write(f"customize_update_html.py: gzip \'data/customized_{filename}\' to \'.pio/data/{filename}.gz\'\n")
            outputFile.writelines(inputFile)
    with open('.pio/data/' + filename + '.timestamp', 'w', -1, 'utf-8') as timestampFile:
        timestampFile.write(str(os.path.getmtime('data/' + filename)))