#
# spec file for package mined
#
# Copyright (c) 2010 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

# norootforbuild


Name:           mined
%if 0%{?suse_version}
BuildRequires:  update-desktop-files
%endif
License:        GPL-3
Group:          Productivity/Editors/Other
AutoReqProv:    on
Version:        2014.24.2
Release:        201424
Url:            http://towo.net/mined/
Source0:        http://towo.net/mined/download/mined-%{version}.tar.gz
#Patch1:	test.patch
#%%if 0%{?suse_version}
#Patch2:	suse.patch
#%%endif
#%%if 0%{?fedora_version} == 13
#Patch3:	fedora13.patch
#%%endif
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Summary:        Powerful Text Editor with Extensive Unicode and CJK Support

%description
Mined is a powerful text editor with a comprehensive yet concise and 
easy-to-use user interface supporting modern interaction paradigms, 
and fast, small-footprint behaviour.

Mined provides both extensive Unicode and CJK support offering many 
specific features and covering special cases that other editors 
are not aware of (like auto-detection features and automatic handling 
of terminal variations, or Han character information).
It was the first editor that supported Unicode in a plain-text terminal 
(like xterm or rxvt).

Basically, mined is an editor tailored to reliable and efficient 
editing of plain text documents and programs, with features and 
interactive behaviour designed for this purpose.


Feature Overview

Good interactive features
* Intuitive user interface
* Logical and consistent concept of navigating and editing text 
  (without ancient line-end handling limitations or insert/append confusion)
* Supports various control styles:
  - Editing with command control, function key control, or menu control
  - Navigation by cursor keys, control keys, mouse or scrollbar
* Concise and comprehensive menus (driven by keyboard or mouse)
* "HOP" key paradigm doubles the number of navigation functions 
  that can be most easily reached and remembered by 
  intuitively amplifying or expanding the associated function
* Proper handling of window size changes in any state of interaction

Versatile character encoding support
* Extensive Unicode support, including double-width and combining characters,
  script highlighting, 
  various methods of character input support 
  (mapped keyboard input methods, mnemonic and numeric input),
  supporting CJK, Vietnamese, Hebrew, Arabic, and other scripts
* Character information updated to Unicode 6.0
* Extensive accented character input support, including 
  multiple accent prefix keys
* Support for Greek (monotonic and polytonic)
* Support for Cyrillic accented characters
* Support of bidirectional terminals
* Support of Arabic ligature joining
  on all terminals
* East Asian character set support: handling of major CJK encodings 
  (including GB18030 and full EUC-JP with combining characters)
* Support for a large number of 8 bit encodings 
  (with combining characters for Vietnamese, Thai, Arabic, Hebrew)
* Support of CJK input methods by enhanced keyboard 
  mapping including multiple choice mappings (handled by a pick list menu);
  characters in the pick list being sorted by relevance of Unicode ranges
* Han character information with description and pronunciation
* Auto-detection of text character encoding, edits files with 
  mixed character encoding sections (e.g. mailboxes),
  transparent handling and auto-detection of UTF-16 encoded files
* Auto-detection of UTF-8 / CJK / 8 bit terminal mode and detailed features
  (like different Unicode width and combining data versions)
* Comprehensive and flexible (though standard-conformant) set of 
  mechanisms to specify both text and terminal encodings 
  with useful precedences
* Flexible combination of any text encoding with any terminal encoding
* Encoding support tested with: xterm, mlterm, rxvt, 
  cxterm, kterm, hanterm, 
  KDE konsole, gnome-terminal, Linux console, 
  cygwin console, mintty, PuTTY

Many useful text editing capabilities
* Many text editing features, e.g. paragraph wrapping, 
  auto-indentation and back-tab, smart quotes (with 
  quotation marks style selection and auto-detection) 
  and smart dashes
* Search and replacement patterns can have multiple lines
* Cross-session paste buffer (copy/paste between multiple 
  - even subsequent or remote - invocations of mined)
* Optional Unicode paste buffer mode with implicit conversion
* Marker stack for quick return to previous text positions
* Multiple paste buffers (emacs-style)
* Optional rectangular copy/paste area
* Interactive selection highlighting (with mouse or keyboard selection), 
  standard dual-mode Del key behaviour
* Program editing features, HTML support and syntax highlighting, 
  identifier and function definition search, also across files; 
  structure input support
* Text and program layout features; auto-indentation and 
  undent function (back-tab), numbered item justification
* Systematic text and file handling safety, avoiding loss of data
* Visible indications of special text contents 
  (TAB characters, different line-end types, character 
  codes that cannot be displayed in the current mode)
* Full binary transparent editing with visible indications 
  (illegal UTF-8 or CJK, mixed line end types, NUL characters, ...)
* Print function that works in all text encodings
* Optional password hiding
* Optional emacs command mode

Small-footprint operation, portability and interworking
* Plain text mode (terminal) operation
* Optimized use of terminal features for a wide range of terminals,
  including large terminal support (2015x2015) or recent xterm and mintty
* Instant start-up
* Runs on many platforms: Unix (Linux/Sun/HP/BSD/Mac and more),
  DOS (djgpp), Windows (cygwin, Interix)
* Makefiles also support legacy systems

Author:
-------
    Thomas Wolff <mined@towo.net>

%prep
%setup0 -q -n mined-%{version}
#%%patch1 -p0
#%%if 0%{?suse_version}
#%%patch2
#%%endif
#%%if 0%{?fedora_version} == 13
#%%patch3
#%%endif

%build
make OPT="$RPM_OPT_FLAGS -fno-strict-aliasing" -C src -f makefile.linux USRLIBDIR=%{_libdir} ROOTLIBDIR=/%{_lib}

%install
[ "$RPM_BUILD_ROOT" != "/" ] && [ -d $RPM_BUILD_ROOT ] && rm -rf $RPM_BUILD_ROOT;
make INSTALLROOT=$RPM_BUILD_ROOT install
mkdir -p $RPM_BUILD_ROOT%{_defaultdocdir}/%{name}
for i in $RPM_BUILD_ROOT/%{_datadir}/mined/*
do
       ln -sf ${i##${RPM_BUILD_ROOT}} \
              $RPM_BUILD_ROOT/%{_defaultdocdir}/%{name}/
done
%if 0%{?suse_version}
%suse_update_desktop_file -i %name ConsoleOnly TextEditor
%endif

%clean
#[ "$RPM_BUILD_ROOT" != "/" ] && [ -d $RPM_BUILD_ROOT ] && rm -rf $RPM_BUILD_ROOT;

%files
%defattr(-, root, root)
%doc %{_defaultdocdir}/%{name}/
%{_bindir}/*
%{_mandir}/man1/*.1*
%dir %{_datadir}/mined/
%{_datadir}/mined/*
/usr/share/applications/%name.desktop
/usr/share/pixmaps/%name.xpm
%if 0%{?suse_version}
%endif

%changelog
