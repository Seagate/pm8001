#This is spec file for pm8001 rpm

# Default to build current running kernel.  To build
# for a different kernel use --define 'KVER <kernel>'
%{?!KVER: %{expand:
%global KVER %(uname -r)
}}

%{?!BUILD_VER: %{expand:
%global BUILD_VER F08
}}


BuildRoot:              %{_tmppath}/%{name}-root
Summary: 		GNU pm8001
License: 		GPL
Name: 			pm8001
Version:                0.1.36.%{BUILD_VER}
Release: 		1
Source0: 		%{name}-%{version}_src.tar.bz2
Prefix:                 /lib 
Group: 			kernel/drivers


%description
The GNU pm8001 driver build and install pm8001 SAS HBA driver to Linux kernel
%prep
%setup -c 
%build
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir $RPM_BUILD_ROOT
install -D pm8001.ko $RPM_BUILD_ROOT/lib/modules/%{KVER}/updates/kernel/drivers/scsi/pm8001.ko
install -D pm8001install $RPM_BUILD_ROOT/usr/share/doc/pm8001/pm8001install
install -D release.txt $RPM_BUILD_ROOT/usr/share/doc/pm8001/release.txt
install -D aap1img.bin $RPM_BUILD_ROOT/lib/firmware/aap1img.bin
install -D iopimg.bin $RPM_BUILD_ROOT/lib/firmware/iopimg.bin
install -D ilaimg.bin $RPM_BUILD_ROOT/lib/firmware/ilaimg.bin
install -D istrimg.bin  $RPM_BUILD_ROOT/lib/firmware/istrimg.bin

%clean
rm -rf $RPM_BUILD_ROOT

%post
# "weak modules" support
# Suse
if [ -x /usr/lib/module-init-tools/weak-modules ]; then
    rpm -ql %{name}-%{version} | grep '\.ko$' |
        /usr/lib/module-init-tools/weak-modules --add-modules
fi
# RedHat
if [ -x /sbin/weak-modules ]; then
    rpm -ql %{name}-%{version} | grep '\.ko$' |
        /sbin/weak-modules --add-modules
fi

modprobe -r -q pm8001
modprobe libsas
depmod -a
modprobe pm8001
modinfo pm8001

%preun
rpm -ql %{name}-%{version} | grep '\.ko$' > /var/run/%{name}-modules

%postun
rmdir /usr/share/doc/%{name} 2>/dev/null || true
if [ "$1" = "0" ] ; then
  rmmod %{name}
fi

# "weak modules" support
# Suse
if [ -x /usr/lib/module-init-tools/weak-modules ]; then
    cat /var/run/%{name}-modules | grep '\.ko$' |
        /usr/lib/module-init-tools/weak-modules --remove-modules
fi
# RedHat
if [ -x /sbin/weak-modules ]; then
    cat /var/run/%{name}-modules | grep '\.ko$' |
        /sbin/weak-modules --remove-modules
fi
depmod -a
rm /var/run/%{name}-modules

%files
%defattr(-,root,root,-)
/lib/modules/%{KVER}/updates/kernel/drivers/scsi/pm8001.ko
/lib/firmware/aap1img.bin
/lib/firmware/iopimg.bin
/lib/firmware/istrimg.bin
/lib/firmware/ilaimg.bin

%doc %attr(0444,root,root) 
/usr/share/doc/pm8001/pm8001install
/usr/share/doc/pm8001/release.txt
