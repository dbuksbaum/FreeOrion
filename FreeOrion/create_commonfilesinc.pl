#!/usr/bin/perl -w
$DEBUG = 0;
open AMFILE, "Makefile.am";

my $in_common = 0;
my %SPECIAL; # List of files that compile differently for SERVER and CLIENT
my %COMMON;  # List of files that are identical for SERVER and CLIENT

while (<AMFILE>) {
    $in_common = 1 if s/^\s*COMMON_FILES\s*=\s*//;
    next if !$in_common;
    $finished = !s/\\\w*$//;
    foreach $File (split / /,$_) {
	chomp $File;
	#remove trailing whitespace
	$File =~ s/\s+$//;
	next if ($File =~ m/^\s*$/);
	(open FILE, $File) or die "File '$File' specified in COMMON_FILES cannot be read";
	while (<FILE>) {
	    if (m/\s*#\s*if(?:n?def)? \s*FREEORION_BUILD_([^\s]+)/) {
		$SPECIAL{$File} = 1;
		goto done;
	    }
	}
	$COMMON{$File} = 1;
      done:
	close FILE;
    }
    last if $finished;
}

close AMFILE;

#foreach $FILE (keys %COMMON) { print "\"$FILE\"\n"; }

# Since some headers may contain inline-template definitions 
# that can be compiled in conditionally, we need to check for all files
# if it depends on (#include's). If one files
# is or depends on such a conditionally compiled file, we must also move 
# the "#includer" to SPECIAL
# find and store dependencies
foreach $filename (keys %COMMON) {
    (open FILE, $filename) or die "$filename: Read error. This worked a little while ago. I'm confused...";
    my @DepThisFile;
    while (<FILE>) {
	if (m/\s*#\w*include\s*(<|")([^>"]+)\1\s*$/) {
	    ($inc = $2) =~ s|^.*/([^/]+)$|$1|;
	    push @DepThisFile, $inc;
	}
    }
    $Depends{$filename} = "@DepThisFile";
    close FILE;
}
	

# now move all files to %SPECIAL that depend on a file already in %SPECIAL

### PATHNAMES ARE IGNORED HERE! IT IS ASSUMED THAT FILENAMES ARE UNIQUE
### BETWEEN DIRECTORIES!

recheck_deps:
$changed = 0;
FILECHECK: while ( ($filename,$deplist) = each(%Depends) ) {
    foreach $fn (split (/ /,$deplist)) {
	if (grep {/$fn/} (keys %SPECIAL)) {
	    # Move filename from COMMON to SPECIAL
	    $SPECIAL{$filename} = 1;
	    delete $COMMON{$filename};

	    # We do not need to check this file anymore
	    delete $Depends{$filename};
	    print "Added $filename, it depends on $fn\n" if $DEBUG;
	    $changed = 1;
	    next FILECHECK;
	}
    }
}

goto recheck_deps if $changed;

open OUTPUT,">common_files.inc" or die ("Error creating common_files.inc");
print OUTPUT "# for emacs: -*- makefile -*-\n";
print OUTPUT "# Compile shared sources only once\n" if (scalar keys %COMMON);
foreach $filename (keys %COMMON) {
    use File::Basename;
    my $basename = basename($filename);
    my $dir = dirname($filename);
    my $no_ext = $basename;
    $no_ext =~ s/\..+?$//;
    foreach $prefix (("client","ai")) {
	foreach $ext (("o","obj")) { # Do not use $(OBJEXT) to avoid warnings about overridden makerules.
	    print OUTPUT<<EOF
$dir/$prefix-$no_ext.$ext: $dir/server-$no_ext.$ext
\t(cd $dir;\$(LN_S) server-$no_ext.$ext $prefix-$no_ext.$ext)
EOF
;
	}
    }
}

foreach $filename (keys %SPECIAL) {
    print OUTPUT "# $filename has to be compiled differently for server/client\n";
}
close OUTPUT;
