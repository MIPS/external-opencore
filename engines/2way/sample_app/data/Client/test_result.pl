#!/usr/bin/perl

my $switch;
my $filesnames;
my $testoutput;
my $delimitor=',';
my $lineDelimit=\n;

$filesnames = $ARGV[0];
	
open( FILE, "< $filesnames" ) or die "Can't open $filenames : $!";

open(OUTFILE, ">output_pv_2way.csv")or die "Can't open $filenames : $!";

$numTestsRun = 1;
@lines="Test case, Init complete status, Connect complete status, Disconnect complete status, Reset complete status, Memory Leaks ";

print OUTFILE "@lines\n\n";
 


while( <FILE> )
{
	s/#.*//;            # ignore comments by erasing them
    next if /^(\s)*$/;  # skip blank lines

    chomp;              # remove trailing newline characters

	$linedata = $_;

	
	if (/Processing File:/)
	{
		  @lines=" ";
		  my $null = substr $linedata, 16, 16;
		  !#print "\n \n $null,";
		  push @lines, $null;    # push the data line onto the array
		  push @lines, $delimitor;
	}
	
	if (/Init complete status /)
	{
		  my $null = substr $linedata, 21, 3;
		 !# print "\n \n $null,";
		  push @lines, $null;    # push the data line onto the array
		  push @lines, $delimitor;
	}

   
   if (/Connect complete status /)
	{

		  my $null = substr $linedata, 24, 3;
		!#  print "$null,";
		  push @lines, $null;    # push the data line onto the array
		  push @lines, $delimitor;
	}



	if (/Disconnect complete status /)
	{

		  my $null = substr $linedata, 27, 3;
		!#  print "$null,";
		  push @lines, $null;    # push the data line onto the array
		  push @lines, $delimitor;
	}


	if (/Reset complete status /)
	{

		  my $null = substr $linedata, 22, 3;
		 !# print "$null,";
		  push @lines, $null;    # push the data line onto the array
		  push @lines, $delimitor;
	}

	if (/Memory leaks detected!/)
	{

		  my $null = "Yes";
		 !# print "$null,";
		  push @lines, $null;    # push the data line onto the array
		  push @lines, $delimitor;
	}

	if(/%%%%%END%%%%%/)
	{
		print OUTFILE "@lines\n\n";
	}
	
   	$numTestsRun++;
}
close OUTFILE;
close FILE;

