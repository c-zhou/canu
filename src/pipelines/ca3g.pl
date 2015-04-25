#!/usr/bin/perl

use strict;

use FindBin;
use Cwd;

use lib "$FindBin::RealBin";
use lib "$FindBin::RealBin/ca3g/lib/perl5";
use lib "$FindBin::RealBin/ca3g/lib64/perl5";

use File::Path qw(make_path remove_tree);

use Carp;
#use FileHandle;

use POSIX "ceil";
use POSIX "floor";
use POSIX "sys_wait_h";  #  waitpid()

use ca3g::Defaults;
use ca3g::Execution;

use ca3g::Gatekeeper;
use ca3g::Meryl;
use ca3g::OverlapInCore;
use ca3g::OverlapMhap;
use ca3g::OverlapStore;

use ca3g::CorrectReads;

use ca3g::OverlapBasedTrimming;

use ca3g::OverlapErrorAdjustment;
use ca3g::Unitig;
use ca3g::Consensus;
use ca3g::Output;


my $bin = undef;  #  Path to binaries, set once in main.
my $cmd = undef;  #  Temporary string passed to system().
my $wrk = undef;  #  Path to our assembly directory.
my $asm = undef;  #  Name of our assembly.

my %global;       #  Global parameters
my %synops;       #  Global parameters - description
my %synnam;       #  Global parameters - case sensitive name

my @specFiles;    #  Files of specs
my @specOpts;     #  Command line specs
my @inputFiles;   #  Command line inputs, later inputs in spec files are added

print STDERR "-- Detected ", getNumberOfCPUs(), " CPUs and ", getPhysicalMemorySize(), " gigabytes of memory.\n";

#  Initialize our defaults.

setDefaults();

#  Check for the presence of a -options switch BEFORE we do any work.
#  This lets us print the default values of options.

foreach my $arg (@ARGV) {
    if ($arg eq "-options") {
        setGlobal("options", 1);
        printHelp($bin);
    }
}

#  The first arg must be the mode of operation.  Currently three modes
#  are supported: correct, trim, assemble.  The modes only affect the
#  steps taken in the 'pipeline' at the bottom of this file.

my $mode = shift @ARGV;
my $step = $mode;

if (($mode ne "run") &&
    ($mode ne "correct") &&
    ($mode ne "trim") &&
    ($mode ne "assemble")) {
    setGlobal("help", "ERROR: first parameter must be the mode of operation (run, correct, trim or assemble).\n");
    printHelp($bin);
    exit(1);
}

addCommandLineOption($mode);

#  At some pain, we stash the original options for later use.  We need
#  to use these when we resubmit ourself to the grid.  We can't simply dump
#  all of @ARGV into here, because we need to fix up relative paths.


#  Enabling/disabling algorithm features is done through library features
#  set in the input gkp files.  This is inconvenient, as you cannot easily
#  change the algorithm without rebuilding gkpStore.  This is flexible, letting
#  you disable an algorithm, or use different parameters for different reads.


while (scalar(@ARGV)) {
    my $arg = shift @ARGV;

    if      ($arg =~ m/^-d/) {
        $wrk = shift @ARGV;
        $wrk = "$ENV{'PWD'}/$wrk" if ($wrk !~ m!^/!);
        addCommandLineOption("-d \"$wrk\"");

    } elsif ($arg eq "-p") {
        $asm = shift @ARGV;
        addCommandLineOption("-p \"$asm\"");

    } elsif ($arg eq "-s") {
        my $spec = shift @ARGV;
        $spec = "$ENV{'PWD'}/$spec" if ($spec !~ m!^/!);

        push @specFiles, $spec;

        addCommandLineOption("-s \"$spec\"");


    } elsif ($arg eq "-version") {
        setGlobal("version", 1);

    } elsif ($arg eq "-options") {
        #  Do nothing.  Handled above, but we still need to process it here.
        #setGlobal("options", 1);

    } elsif (($arg eq "-pacbio-raw")       ||
             ($arg eq "-pacbio-corrected") ||
             ($arg eq "-nanopore-raw")     ||
             ($arg eq "-nanopore-corrected")) {
        while (-e $ARGV[0]) {
            my $file = shift @ARGV;

            $file = "$ENV{'PWD'}/$file"  if ($file !~ m!^/!);

            push @inputFiles, "$arg:$file";
            addCommandLineOption("$arg \"$file\"");
        }

    } elsif (-e $arg) {
        $arg = "$ENV{'PWD'}/$arg" if ($arg !~ m!^/!);
        push @inputFiles, $arg;
        addCommandLineOption("\"$arg\"");

    } elsif ($arg =~ m/=/) {
        push @specOpts, $arg;
        addCommandLineOption("\"$arg\"");

    } else {
        setGlobal("help",
                  getGlobal("help") . "File not found or invalid command line option '$arg'\n");
    }
}


setGlobal("help", getGlobal("help") . "ERROR:  Assembly name prefix not supplied with -p.\n") if (!defined($asm));
setGlobal("help", getGlobal("help") . "ERROR:  Directory not supplied with -d.\n")            if (!defined($wrk));


$bin = getBinDirectory();

#@inputFiles = setParametersFromFile("$bin/spec/runCA.default.specFile", @inputFiles)   if (-e "$bin/spec/runCA.default.specFile");
#@inputFiles = setParametersFromFile("$ENV{'HOME'}/.runCA",              @inputFiles)   if (-e "$ENV{'HOME'}/.runCA");


#  For each of the spec files, parse it, setting parameters and remembering any input files discovered.

foreach my $specFile (@specFiles) {
    @inputFiles = setParametersFromFile($specFile, @inputFiles);
}

#  Set parameters from the command line.

setParametersFromCommandLine(@specOpts);

#  Finish setting parameters.

checkParameters($bin);

#  If anything complained, global{'help'} will be defined, and we'll print help (and the error) and
#  stop.

printHelp($bin);

#  Fail immediately if we run the script on the grid, and the gkpStore directory doesn't exist and
#  we have no input files.  Without this check we'd fail only after being scheduled on the grid.

if ((! -d "$wrk/$asm.gkpStore") &&
    (scalar(@inputFiles) == 0)) {
    caFailure("no input files specified, and store not already created", undef);
}

#  Check that we were supplied a work directory, and that it exists, or we can create it.

caFailure("no run directory (-d option) specified", undef)  if (!defined($wrk));

make_path("$wrk")             if (! -d "$wrk");
make_path("$wrk/runCA-logs")  if (! -d "$wrk/runCA-logs");

caFailure("run directory (-d option) '$wrk' doesn't exist and couldn't be created", undef)  if (! -d $wrk);

#  This environment variable tells the binaries to log their execution in runCA-logs/

$ENV{'AS_RUNCA_DIRECTORY'} = $wrk;

#  Report the parameters used.

writeLog($wrk);

#  Submit ourself for grid execution?  If not grid enabled, or already running on the grid, this
#  call just returns.  The arg MUST be undef.

submitScript($wrk, $asm, undef);





#
#  When doing 'run', this sets options for each stage.
#    - overlapper 'mhap' for correction, 'ovl' for trimming and assembly.
#    - consensus 'falconpipe' for correction, 'utgcns' for assembly.  No consensus in trimming.
#    - errorRates 15% for correction and 2% for trimming and assembly.  Internally, this is
#      multiplied by three for obt, ovl, cns, etc.
#

sub setOptions ($$) {
    my $mode = shift @_;
    my $step = shift @_;

    make_path("$wrk/correction")  if ((! -d "$wrk/correction") && ($step eq "correct"));
    make_path("$wrk/trimming")    if ((! -d "$wrk/trimming")   && ($step eq "trim"));

    return($mode)  if ($mode ne "run");

    if ($step eq "correct") {
        setGlobal("overlapper", "mhap");
        setGlobal("consensus",  "falconpipe");

        setErrorRate(0.15);

        return($step);
    }

    if ($step eq "trim") {
        setGlobal("overlapper", "ovl");

        setErrorRate(0.02);

        return($step);
    }

    if ($step eq "assemble") {
        setGlobal("overlapper", "ovl");
        setGlobal("consensus",  "utgcns");

        setErrorRate(0.02);

        return($step);
    }
}

#
#  Pipeline piece
#

sub overlap ($$$) {
    my $wrk  = shift @_;
    my $asm  = shift @_;
    my $mode = shift @_;

    my $ovlType = ($mode eq "assemble") ? "normal" : "partial";

    if (getGlobal('overlapper') eq "mhap") {
        mhapConfigure($wrk, $asm, $ovlType);
        
        mhapPrecomputeCheck($wrk, $asm, $ovlType, 0);
        mhapPrecomputeCheck($wrk, $asm, $ovlType, 1);

        mhapCheck($wrk, $asm, $ovlType, 0);  #  this also does mhapReAlign
        mhapCheck($wrk, $asm, $ovlType, 1);

    } else {
        overlapConfigure($wrk, $asm, $ovlType);
        overlapCheck($wrk, $asm, $ovlType, 0);
        overlapCheck($wrk, $asm, $ovlType, 1);
    }

    createOverlapStore($wrk, $asm, getGlobal("ovlStoreMethod"));
}

#
#  Begin pipeline
#

if (setOptions($mode, "correct") eq "correct") {
    gatekeeper("$wrk/correction", $asm, @inputFiles);
    meryl("$wrk/correction", $asm);
    overlap("$wrk/correction", $asm, "correct");

    buildCorrectionLayouts("$wrk/correction", $asm);

    generateCorrectedReads("$wrk/correction", $asm, 0);
    generateCorrectedReads("$wrk/correction", $asm, 1);

    dumpCorrectedReads("$wrk/correction", $asm);

    undef @inputFiles;
    push  @inputFiles, "-pacbio-corrected:$wrk/correction/$asm.correctedReads.fastq";
}


if (setOptions($mode, "trim") eq "trim") {
    gatekeeper("$wrk/trimming", $asm, @inputFiles);
    meryl("$wrk/trimming", $asm);
    overlap("$wrk/trimming", $asm, "trim");

    trimReads  ("$wrk/trimming", $asm);
    splitReads ("$wrk/trimming", $asm);
    dumpReads  ("$wrk/trimming", $asm);
    #summarizeReads("$wrk/trimming", $asm);

    undef @inputFiles;
    push  @inputFiles, "-pacbio-corrected:$wrk/trimming/$asm.trimmedReads.fastq";
}


if (setOptions($mode, "assemble") eq "assemble") {
    gatekeeper($wrk, $asm, @inputFiles);
    meryl($wrk, $asm);
    overlap($wrk, $asm, "assemble");

    #readErrorDetection($wrk, $asm);
    overlapErrorAdjustment($wrk, $asm);

    unitig($wrk, $asm);

    consensusConfigure($wrk, $asm);
    consensusCheck($wrk, $asm, 0);
    consensusCheck($wrk, $asm, 1);

    outputLayout($wrk, $asm);
    outputConsensus($wrk, $asm);
}

exit(0);