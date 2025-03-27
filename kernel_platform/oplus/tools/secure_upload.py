# secure_upload.py
import subprocess
import sys
import argparse
import json

def parse_cmd_args():

    parser = argparse.ArgumentParser(description="secure curl upload")
    parser.add_argument('-u', '--user', type=str, help='upload user name', default='')
    parser.add_argument('-p', '--password', type=str, help='upload password', default='')
    parser.add_argument('-l', '--local', type=str, help='local_path', default='')
    parser.add_argument('-r', '--remote', type=str, help='remote_path', default='')
    args = parser.parse_args()

    print('local:', args.local, '\nremote:', args.remote)
    return args

def secure_curl_upload(local_path, remote_path, user="", password=""):
    curl_command = [
            "curl",
            "-u", "{}:{}".format(user, password),
            "-T", local_path,
            remote_path
    ]

    try:
        result = subprocess.run(curl_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)

        # Check if stdout contains a JSON error message
        try:
            stdout_json = json.loads(result.stdout)
            if 'errors' in stdout_json:
                print("Failed to upload {} to {}".format(local_path, remote_path))
                for error in stdout_json['errors']:
                    print("Error: Status {} - Message: {}".format(error.get('status', 'Unknown'), error.get('message', 'No message')))
                    if "Unauthorized" in error.get('message', 'No message'):
                        print("please input user name and password")
            else:
                if result.returncode != 0:
                    print("Failed to upload {} to {}".format(local_path, remote_path))
                    print("Error: {}".format(result.stderr))
                else:
                    print("Successfully uploaded {} to {}".format(local_path, remote_path))
        except json.JSONDecodeError:
            pass  # stdout is not JSON, might be normal output

    except Exception as e:
        print("Exception occurred while uploading {} to {}: {}".format(local_path, remote_path, e))

if __name__ == "__main__":

    args = parse_cmd_args()
    if args.user and args.password:
        secure_curl_upload(args.local, args.remote, args.user, args.password)
    else:
        secure_curl_upload(args.local, args.remote)