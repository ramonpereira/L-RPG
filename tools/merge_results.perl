#!/usr/bin/perl -w

use strict;
use warnings;

my $from_problem_count = shift(@ARGV);
my $problem_count = shift(@ARGV);
#my $marvin_directory = "/home/bram/projects/marvin-bram/latest_results";
my $lhs_directory = "/home/bram/projects/MyPOP/trunk/latest_heuristics_results_all_goals_removed_ff";
my $rhs_directory = "/home/bram/planners/FF-X2/FF-X/latest_heuristics_results";
#my $marvin_directory = "/home/bram/projects/marvin-bram/results";
#my $marvin_directory = "/home/bram/projects/MyPOP/tags/ICAPS-2012-speed-1.0/trunk/latest_results";
my $output_directory = "merged_results_all_goals_removed_ff";

system ("mkdir -p $output_directory");

opendir(DIR, $lhs_directory);
my @FILES = readdir(DIR);
closedir(DIR);

foreach my $file (@FILES)
{
	unless ($file =~ m/.*dat\Z/) { next; }

	# Find the corresponding file in the marvin directory.
	my $rhs_file_name = $rhs_directory."/".$file;
	if (-e $rhs_file_name)
	{
		print "Found: $rhs_file_name!\n";
	} else {
		print "Could not find: $rhs_file_name - skipping!\n";
		next;
	}

	# Get the 2nd column of every row in each file and concatenate them in a single file.
	open OUTPUT, ">", $output_directory."/".$file;
	open LOLLIPOP_RESULT, "<", $lhs_directory."/".$file or die $!;
	open MARVIN_RESULT, "<", $rhs_file_name or die $!;

	my @lollipop_lines = <LOLLIPOP_RESULT>;
	my @marvin_lines = <MARVIN_RESULT>;

	unless (scalar (@lollipop_lines) eq scalar (@marvin_lines) )
	{
		print "$file The number of data points are not equal ".scalar (@lollipop_lines)." v.s. ".scalar (@marvin_lines)."!\n";
		exit;
	}

	for (my $i = 0; $i < scalar (@lollipop_lines); $i++)
	{
		my $lollipop_line = $lollipop_lines[$i];
		my $marvin_line = $marvin_lines[$i];

		# Get the 2nd column of both lines.
		my @lollipop_columns = split(' ', $lollipop_line);
		my @marvin_columns = split(' ', $marvin_line);

		print OUTPUT "$lollipop_columns[1] $marvin_columns[1]\n";
	}

	close OUTPUT;
	close LOLLIPOP_RESULT;
	close MARVIN_RESULT;
}
