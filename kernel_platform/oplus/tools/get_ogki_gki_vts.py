import ftplib
import re
import os
from zipfile import ZipFile
import subprocess
import argparse

def connect_ftp(host, user=None, password=None):
    try:
        ftp = ftplib.FTP(host)
        if user and password:
            ftp.login(user=user, passwd=password)
        else:
            ftp.login()
        print("Connected to FTP server: {}".format(host))
        return ftp
    except ftplib.all_errors as e:
        print("Failed to connect: {}".format(e))
        return None

def get_latest_folder(ftp, directory):
    try:
        ftp.cwd(directory)
        entries = []
        ftp.retrlines('LIST', entries.append)
        print("Retrieved directory listing for: {}".format(directory))
    except ftplib.error_perm as e:
        print("Error accessing directory: {}".format(e))
        return None

    # Updated pattern to match "V-20240803*" format
    pattern = re.compile(r'[d-]{1}[rwx-]{9}\s+\d+\s+\w+\s+\w+\s+\d+\s+\w+\s+\d+\s+\d+:\d+\s+(V-\d{8}\w*)')

    latest_file = None
    latest_date = None
    latest_class = None

    for entry in entries:
        match = pattern.search(entry)
        if match:
            name = match.group(1)
            file_class, file_date = name.split('-')[0], name.split('-')[1][:8]
            if (latest_class is None or file_class > latest_class) or \
               (file_class == latest_class and file_date > (latest_date or '')):
                latest_class = file_class
                latest_date = file_date
                latest_file = name

    if latest_file:
        print("Found latest folder: {}".format(latest_file))
    else:
        print("No matching directory found.")
    #latest_file = "U-20240731"
    return latest_file

def find_gsi_zip(ftp, directory):
    try:
        ftp.cwd(directory)
        file_list = ftp.nlst()
        print("Retrieved file listing for: {}".format(directory))
    except ftplib.error_perm as e:
        print("Error accessing directory: {}".format(e))
        return None

    gsi_pattern = re.compile(r'.*gsi_arm64-img.*\.zip')

    for file_name in file_list:
        if gsi_pattern.match(file_name):
            print("Found matching GSI ZIP file: {}".format(file_name))
            return file_name

    print("No matching GSI ZIP file found.")
    return None

def download_file(ftp, ftp_path, local_path):
    try:
        with open(local_path, 'wb') as f:
            ftp.retrbinary('RETR {}'.format(ftp_path), f.write)
        print("Downloaded: {} to {}".format(ftp_path, local_path))
    except ftplib.all_errors as e:
        print("Failed to download file: {}".format(e))

def extract_zip(file_path, extract_to):
    try:
        with ZipFile(file_path, 'r') as zip_ref:
            zip_ref.extractall(extract_to)
        print("Extracted: {} to {}".format(file_path, extract_to))
    except Exception as e:
        print("Failed to extract file: {}".format(e))

def get_tools_path(relative_path):
    base_path = os.path.dirname(os.path.realpath(__file__))
    full_path = os.path.join(base_path, relative_path)
    #print("base_path {} relative_path {} full_path {}".format(base_path, relative_path, full_path))
    return full_path

def parse_cmd_args():
    parser = argparse.ArgumentParser(description="gsi image check")
    parser.add_argument('--ftp_host', type=str, help='ftp host address', default='172.16.53.101')
    parser.add_argument('--ftp_user', type=str, help='ftp user', default=None)
    parser.add_argument('--ftp_password', type=str, help='ftp password', default=None)
    parser.add_argument('--ftp_directory', type=str, help='ftp_directory', default='/public/olt_resource/GoogleTools/DailyBuild/')
    parser.add_argument('-b', '--bootimg', type=str, help='boot image path', default='boot.img')
    parser.add_argument('-a', '--approved', type=str, help='approved ogki builds', default='out/system/etc/kernel/approved-ogki-builds.xml')
    parser.add_argument('--download_dir', type=str, help='download dir', default=get_tools_path('gsi'))
    parser.add_argument('--extract_dir', type=str, help='extract_dir', default=get_tools_path('upack_gsi'))
    parser.add_argument('--unpack_tool', type=str, help='unpack_tool', default=get_tools_path('unpack_image.py'))
    parser.add_argument('--unpack_image', type=str, help='unpack_image', default=get_tools_path('system.img'))

    args = parser.parse_args()
    args.unpack_image = os.path.join(args.extract_dir, "system.img")

    print('ftp_host:', args.ftp_host, '\nftp_directory:', args.ftp_directory, '\nbootimg:', args.bootimg,
      '\napproved:', args.approved, '\ndownload_dir:', args.download_dir,
      '\nextract_dir:', args.extract_dir, '\nunpack_tool:', args.unpack_tool,'\nunpack_image:', args.unpack_image)
    return args

def init_vts_env(args):
    if not os.path.exists(args.download_dir):
        os.makedirs(args.download_dir)
    if not os.path.exists(args.extract_dir):
        os.makedirs(args.extract_dir)

def ftp_download(args):
    ftp = connect_ftp(args.ftp_host, args.ftp_user, args.ftp_password)
    if not ftp:
        return

    latest_folder = get_latest_folder(ftp, args.ftp_directory)
    if latest_folder:
        zip_file_name = find_gsi_zip(ftp, os.path.join(args.ftp_directory, latest_folder))
        if zip_file_name:
            local_zip_path = os.path.join(args.download_dir, zip_file_name)
            ftp_path = os.path.join(args.ftp_directory, latest_folder, zip_file_name)
            print("Starting download from: {} to local path: {}".format(ftp_path, local_zip_path))
            download_file(ftp, ftp_path, local_zip_path)
            extract_zip(local_zip_path, args.extract_dir)
        else:
            print("No matching GSI ZIP file found.")
    else:
        print("No matching directory found.")
    ftp.quit()

def unpack_image(args):
    command = ['python3', args.unpack_tool, '-i', args.unpack_image]
    subprocess.run(command)

def analysis_approve(args):
    command = ['python3', get_tools_path('ack_lts_spl_check.py'), '-b', args.bootimg, '-a', args.approved]
    subprocess.run(command)

def check_result(args):
    file_name = get_tools_path('kernel_version.txt')
    print(f"file_name: {file_name}")
    with open(file_name, 'r') as file:
        lines = file.readlines()

    for line in lines:
        line = line.strip()
        if line.startswith('LINUX_KERNEL_FULL_VERSION:'):
            value = line.split(':')[1]
            print(f"LINUX_KERNEL_FULL_VERSION: {value}")
        elif line.startswith('LINUX_KERNEL_FULL_VERSION_SHA256:'):
            value = line.split(':')[1]
            print(f"LINUX_KERNEL_FULL_VERSION_SHA256: {value}")
        elif line.startswith('LINUX_KERNEL_FULL_VERSION_OGKI_VTS_CHECK:'):
            value = line.split(':')[1]
            print(f"LINUX_KERNEL_FULL_VERSION_OGKI_VTS_CHECK: {value}")
        elif line.startswith('google_bug_id:'):
            value = line.split(':')[1]
            print(f"google_bug_id: {value}")

def main():
    args = parse_cmd_args()

    init_vts_env(args)
    ftp_download(args)
    unpack_image(args)
    analysis_approve(args)
    check_result(args)

if __name__ == "__main__":
    main()
