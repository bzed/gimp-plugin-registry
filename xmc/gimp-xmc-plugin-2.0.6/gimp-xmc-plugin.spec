Name:           gimp-xmc-plugin
Version:        2.0.6
Release:        1%{?dist}
Summary:        X11 Mouse Cursor plug-in for the GIMP

Group:          Applications/Multimedia
License:        GPLv3+
URL:            http://www.gimpstuff.org/content/show.php/X11+Mouse+Cursor+(XMC)+plug-in?content=94503
Source0:        http://www.sutv.zaq.ne.jp/linuz/tks/item/%{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  pkgconfig gimp-devel >= 2.6 libXcursor-devel glibc-devel glib2-devel
Requires:       gimp >= 2.6

%description
X11 Mouse Cursor plug-in for GIMP (or gimp-xmc-plugin) enable
GIMP to import, export X11 Mouse Cursor.



%prep
%setup -q


%build
%configure
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%doc AUTHORS COPYING ChangeLog README NEWS
%{_libdir}/gimp/2.0/plug-ins/file-xmc


%changelog
* Thu Apr 23 2009 tks - 2.0.6-1%{?dist}
- Version 2.0.6
* Sat Feb 14 2009 tks - 2.0.5-1%{?dist}
- Version 2.0.5
* Sun Dec 07 2008 tks - 2.0.4-1%{?dist}
- Version 2.0.4
* Sat Nov 01 2008 tks - 2.0.3-1.fc10
- Version 2.0.3
* Sun Oct 26 2008 tks - 2.0.1-1.fc10
- Version 2.0.1
* Sat Oct 24 2008 tks - 2.0.0-1.fc10
- Version 2.0.0
* Sat Oct 18 2008 tks - 1.0.0-1.fc9
- Version 1.0.0
* Sun Oct 11 2008 tks - 0.3.0-1.fc9
- Version 0.3.0
* Thu Oct 04 2008 tks - 0.2.2-1.fc9
- Version 0.2.2
* Thu Oct 02 2008 tks - 0.2.1-1.fc9
- Version 0.2.1
* Fri Sep 27 2008 tks - 0.2.0-1.fc9
- Version 0.2.0
* Thu Sep 11 2008 tks - 0.1.1-1.fc9
- Version 0.1.1
* Mon Sep 08 2008 tks - 0.1.0-1.fc9
- Initial release
