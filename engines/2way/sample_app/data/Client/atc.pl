#!/usr/local/bin/perl

my $switch;
my $testexe;
my $filesnames;
my $testoutput = 'test_output_pv_2way.txt';
my $flags;

$switch = $ARGV[0];
if( $switch ) 
{
	$testexe = $ARGV[0];	
	$filesnames = $ARGV[1];	
}

open( FILE, "< $filesnames" ) or die "Can't open $filenames : $!";

if ( -e $testoutput )  
{
unlink "$testoutput";
}

$numTestsRun = 0;
while( <FILE> )
{
	s/#.*//;            # ignore comments by erasing them
    next if /^(\s)*$/;  # skip blank lines

    chomp;              # remove trailing newline characters

    push @lines, $_;    # push the data line onto the array

    my @splitCommand = split(/:;/, $_);

	$testNumber = $splitCommand[0];
	$flags=$splitCommand[2];
	
	$numTestsRun++;
#	print "\n\nStart test case $numTestsRun ....\n";
   
   print "Running $splitCommand[0] ...\n";

#    system( "$testexe $splitCommand[0] 1<&2>> $testoutput");
    system( "$testexe $splitCommand[0]");

#	print "Done....\n";

	open(OUTFILE, ">> $testoutput")or die "Can't open $filenames : $!";

	print OUTFILE "%%%%%END%%%%%\n";
	
	close OUTFILE;	
	
	sleep(2);	
}

close FILE;

print "Number of Test Cases Run: $numTestsRun\n";
print "\nAll Done\n";

system( "perl test_result.pl test_output_pv_2way.txt" );


