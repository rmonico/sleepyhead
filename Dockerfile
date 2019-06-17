from ubuntu:18.04

# copy "SleepyHead-1.0.0-beta-1-Snapshot-Ubuntu-15.04_x64.deb /root/SleepyHead-1.0.0-beta-1-Snapshot-Ubuntu-15.04_x64.deb

run "apt update"

run "apt upgrade"

run "apt install sleepyhead"

run "apt-get -f install"

# RUN "dpkg -i /root/SleepyHead-1.0.0-beta-1-Snapshot-Ubuntu-15.04_x64.deb"

entrypoint ["/bin/bash"]
