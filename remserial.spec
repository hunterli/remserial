%define pfx /opt/freescale/rootfs/%{_target_cpu}

Summary         : The remserial program acts as a communications bridge between a TCP/IP network port and a Linux device such as a serial port. Any character-oriented Linux /dev device will work.
Name            : remserial
Version         : 1.4
Release         : 1
License         : GPL
Vendor          : LPC
Packager        : lht
Group           : Applications/Communications
URL             : http://www.yuan-ying.com
Source          : %{name}-%{version}.tar.gz
BuildRoot       : %{_tmppath}/%{name}
Prefix          : %{pfx}

%Description
%{summary}

%Prep
%setup

%Build
make

%Install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{pfx}/sbin
cp %{name} $RPM_BUILD_ROOT/%{pfx}/sbin/
mkdir -p $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/init.d/
cat << EOF > $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/init.d/%{name}
#!/bin/sh

if [ ! -x /sbin/%{name} ]
then
    exit 0
fi

if [ "\$1" = "stop" -o "\$1" = "restart" ]
then
    echo "Stopping the %{name}: "
    killall %{name}
fi
if [ "\$1" = "start" -o "\$1" = "restart" ]
then
    echo "Starting the %{name}: "
    for i in 0 1 2 3; do
        /sbin/%{name} -d -p 2300\$i -s "9600 raw" /dev/ttySP\$i &
    done
fi
EOF
chmod +x $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/init.d/%{name}

%Clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(-,root,root)
%{pfx}/*
