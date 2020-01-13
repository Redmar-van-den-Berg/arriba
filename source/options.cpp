#include <algorithm>
#include <cstring>
#include <iostream>
#include <libgen.h>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include "common.hpp"
#include "annotation.hpp"
#include "options.hpp"

using namespace std;

string wrap_help(const string& option, const string& text, const unsigned short int max_line_width) {
	istringstream iss(text);
	string result = " " + option + "  ";
	string indent(result.length(), ' ');
	unsigned short int line_width = result.length();
	while (iss && iss.tellg() != -1) {
		if (iss.str().substr(iss.tellg(), 1) == "\n") {
			result += "\n" + indent;
			line_width = indent.length();
		}
		string word;
		iss >> word;
		if (word.length() + line_width > max_line_width) {
			result += "\n" + indent;
			line_width = indent.length();
		}
		result += word + " ";
		line_width += word.length();
	}
	return result + "\n\n";
}

bool output_directory_exists(const string& output_file) {
	if (output_file.empty())
		return false;
	char* output_file_c_str = strdup(output_file.c_str()); // we need to make a copy, because dirname does not take const argument
	char* output_directory = dirname(output_file_c_str);
	struct stat file_info;
	bool result = stat(output_directory, &file_info) == 0 && (file_info.st_mode & S_IFDIR);
	free(output_file_c_str);
	return result;
}

bool validate_int(const char* optarg, int& value, const int min_value, const int max_value) {
	value = atoi(optarg);
	if (string(optarg) != string("0") && value == 0)
		return false;
	return value >= min_value && value <= max_value;
}

bool validate_int(const char* optarg, unsigned int& value, const unsigned int min_value, const unsigned int max_value) {
	int signed_int;
	if (validate_int(optarg, signed_int, min_value, max_value)) {
		value = signed_int;
		return true;
	} else
		return false;
}

bool validate_float(const char* optarg, float& value, const float min_value, const float max_value) {
	value = atof(optarg);
	if (string(optarg) != string("0") && value == 0)
		return false;
	return value >= min_value && value <= max_value;
}

options_t get_default_options() {
	options_t options;

	options.interesting_contigs = "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 X Y";
	for (size_t i = 0; i < FILTERS.size(); ++i)
		if (i != FILTER_none)
			options.filters[FILTERS[i]] = true;
	options.evalue_cutoff = 0.3;
	options.min_support = 2;
	options.max_mismapper_fraction = 0.8;
	options.max_homolog_identity = 0.3;
	options.min_anchor_length = 23;
	options.homopolymer_length = 6;
	options.max_genomic_breakpoint_distance = 100000;
	options.min_read_through_distance = 10000;
	options.print_supporting_reads = false;
	options.print_supporting_reads_for_discarded_fusions = false;
	options.print_fusion_sequence = false;
	options.print_fusion_sequence_for_discarded_fusions = false;
	options.print_peptide_sequence = false;
	options.print_peptide_sequence_for_discarded_fusions = false;
	options.max_kmer_content = 0.6;
	options.fragment_length = 200;
	options.strandedness = STRANDEDNESS_AUTO;
	options.gtf_features = DEFAULT_GTF_FEATURES;
	options.min_spliced_events = 4;
	options.mismatch_pvalue_cutoff = 0.01;
	options.subsampling_threshold = 300;
	options.high_expression_quantile = 0.998;
	options.exonic_fraction = 0.2;

	return options;
}

void print_usage() {

	options_t default_options = get_default_options();
	string valid_filters;
	for (auto i = default_options.filters.begin(); i != default_options.filters.end(); ++i) {
		if (i != default_options.filters.begin())
			valid_filters += ", ";
		valid_filters += i->first;
	}

	cout << endl
	     << "Arriba gene fusion detector" << endl
	     << "---------------------------" << endl
	     << "Version: " << ARRIBA_VERSION << endl << endl
	     << "Arriba is a fast tool to search for aberrant transcripts such as gene fusions. " << endl
	     << "It is based on chimeric alignments found by the STAR RNA-Seq aligner." << endl
	     << endl
	     << "Usage: arriba [-c Chimeric.out.sam] -x Aligned.out.bam \\" << endl
	     << "              -g annotation.gtf -a assembly.fa [-b blacklists.tsv] [-k known_fusions.tsv] \\" << endl
	     << "              -o fusions.tsv [-O fusions.discarded.tsv] \\" << endl
	     << "              [OPTIONS]" << endl
	     << endl
	     << wrap_help("-c FILE", "File in SAM/BAM/CRAM format with chimeric alignments as "
	                  "generated by STAR (Chimeric.out.sam). This parameter is only required, "
	                  "if STAR was run with the parameter '--chimOutType SeparateSAMold'. "
	                  "When STAR was run with the parameter '--chimOutType WithinBAM', it "
	                  "suffices to pass the parameter -x to Arriba and -c can be omitted.")
	     << wrap_help("-x FILE", "File in SAM/BAM/CRAM format with main alignments as "
	                  "generated by STAR (Aligned.out.sam). Arriba extracts candidate reads "
	                  "from this file.")
	     << wrap_help("-g FILE", "GTF file with gene annotation. The file may be gzip-compressed.")
	     << wrap_help("-G GTF_FEATURES", "Comma-/space-separated list of names of GTF features.\n"
	                  "Default: " + default_options.gtf_features)
	     << wrap_help("-a FILE", "FastA file with genome sequence (assembly). "
	                  "The file may be gzip-compressed. An index with the file extension .fai "
	                  "must exist only if CRAM files are processed.")
	     << wrap_help("-b FILE", "File containing blacklisted events (recurrent artifacts "
	                  "and transcripts observed in healthy tissue).")
	     << wrap_help("-k FILE", "File containing known/recurrent fusions. Some cancer "
	                  "entities are often characterized by fusions between the same pair of genes. "
	                  "In order to boost sensitivity, a list of known fusions can be supplied using this parameter. "
	                  "The list must contain two columns with the names of the fused genes, "
	                  "separated by tabs.")
	     << wrap_help("-o FILE", "Output file with fusions that have passed all filters.")
	     << wrap_help("-O FILE", "Output file with fusions that were discarded due to filtering.")
	     << wrap_help("-d FILE", "Tab-separated file with coordinates of structural variants "
	                  "found using whole-genome sequencing data. These coordinates serve to "
	                  "increase sensitivity towards weakly expressed fusions and to eliminate "
	                  "fusions with low evidence.")
	     << wrap_help("-D MAX_GENOMIC_BREAKPOINT_DISTANCE", "When a file with genomic breakpoints "
	                  "obtained via whole-genome sequencing is supplied via the -d parameter, "
	                  "this parameter determines how far a genomic breakpoint may be away from "
	                  "a transcriptomic breakpoint to consider it as a related event. "
	                  "For events inside genes, the distance is added to the end of the gene; "
	                  "for intergenic events, the distance threshold is applied as is. Default: " +
	                  to_string(static_cast<long long unsigned int>(default_options.max_genomic_breakpoint_distance)))
	     << wrap_help("-s STRANDEDNESS", "Whether a strand-specific protocol was used for library preparation, and if so, "
	                  "the type of strandedness (auto/yes/no/reverse). When unstranded data is processed, the strand "
	                  "can sometimes be inferred from splice-patterns. But in unclear situations, stranded "
	                  "data helps resolve ambiguities. Default: " + string((default_options.strandedness == STRANDEDNESS_NO) ? "no" : ((default_options.strandedness == STRANDEDNESS_YES) ? "yes" : ((default_options.strandedness == STRANDEDNESS_REVERSE) ? "reverse" : "auto"))))
	     << wrap_help("-i CONTIGS", "Comma-/space-separated list of interesting contigs. Fusions "
	                  "between genes on other contigs are ignored. Contigs can be specified with "
	                  "or without the prefix \"chr\".\nDefault: " + default_options.interesting_contigs)
	     << wrap_help("-f FILTERS", "Comma-/space-separated list of filters to disable. By default "
	                  "all filters are enabled. Valid values: " + valid_filters)
	     << wrap_help("-E MAX_E-VALUE", "Arriba estimates the number of fusions with a given "
	                  "number of supporting reads which one would expect to see by random chance. "
	                  "If the expected number of fusions (e-value) is higher than this threshold, "
	                  "the fusion is discarded by the 'relative_support' filter. Note: "
	                  "Increasing this threshold can dramatically increase the "
	                  "number of false positives and may increase the runtime "
	                  "of resource-intensive steps. Fractional values are possible. Default: " + to_string(static_cast<long double>(default_options.evalue_cutoff)))
	     << wrap_help("-S MIN_SUPPORTING_READS", "The 'min_support' filter discards all fusions "
	                  "with fewer than this many supporting reads (split reads and discordant "
	                  "mates combined). Default: " + to_string(static_cast<long long unsigned int>(default_options.min_support)))
	     << wrap_help("-m MAX_MISMAPPERS", "When more than this fraction of supporting reads "
	                  "turns out to be mismappers, the 'mismappers' filter "
	                  "discards the fusion. Default: " + to_string(static_cast<long double>(default_options.max_mismapper_fraction)))
	     << wrap_help("-L MAX_HOMOLOG_IDENTITY", "Genes with more than the given fraction of "
	                  "sequence identity are considered homologs and removed by the 'homologs' "
	                  "filter. Default: " + to_string(static_cast<long double>(default_options.max_homolog_identity)))
	     << wrap_help("-H HOMOPOLYMER_LENGTH", "The 'homopolymer' filter removes breakpoints "
	                  "adjacent to homopolymers of the given length or more. Default: " + to_string(static_cast<long long unsigned int>(default_options.homopolymer_length)))
	     << wrap_help("-R READ_THROUGH_DISTANCE", "The 'read_through' filter removes read-through fusions "
	                  "where the breakpoints are less than the given distance away from each other. "
	                  "Default: " + to_string(static_cast<long long unsigned int>(default_options.min_read_through_distance)))
	     << wrap_help("-A MIN_ANCHOR_LENGTH", "Alignment artifacts are often characterized by "
	                  "split reads coming from only one gene and no discordant mates. Moreover, the split reads only "
	                  "align to a short stretch in one of the genes. The 'short_anchor' "
	                  "filter removes these fusions. This parameter sets the threshold in bp for "
	                  "what the filter considers short. Default: " + to_string(static_cast<long long unsigned int>(default_options.min_anchor_length)))
	     << wrap_help("-M MANY_SPLICED_EVENTS", "The 'many_spliced' filter recovers fusions "
	                  "between genes that have at least this many spliced breakpoints. Default: " + to_string(static_cast<long long unsigned int>(default_options.min_spliced_events)))
	     << wrap_help("-K MAX_KMER_CONTENT", "The 'low_entropy' filter removes reads with "
	                  "repetitive 3-mers. If the 3-mers make up more than the given fraction "
	                  "of the sequence, then the read is discarded. Default: " + to_string(static_cast<long double>(default_options.max_kmer_content)))
	     << wrap_help("-V MAX_MISMATCH_PVALUE", "The 'mismatches' filter uses a binomial model "
	                  "to calculate a p-value for observing a given number of mismatches in a read. "
	                  "If the number of mismatches is too high, the read is discarded. Default: " + to_string(static_cast<long double>(default_options.mismatch_pvalue_cutoff)))
	     << wrap_help("-F FRAGMENT_LENGTH", "When paired-end data is given, the fragment length "
	                  "is estimated automatically and this parameter has no effect. But when "
	                  "single-end data is given, the mean fragment length should be specified "
	                  "to effectively filter fusions that arise from hairpin structures. "
	                  "Default: " + to_string(static_cast<long long unsigned int>(default_options.fragment_length)))
	     << wrap_help("-U MAX_READS", "Subsample fusions with more than the given number of "
	                  "supporting reads. This improves performance without compromising sensitivity, "
	                  "as long as the threshold is high. Counting of supporting reads beyond "
	                  "the threshold is inaccurate, obviously. "
	                  "Default: " + to_string(static_cast<long long unsigned int>(default_options.subsampling_threshold)))
	     << wrap_help("-Q QUANTILE", "Highly expressed genes are prone to produce artifacts "
	                  "during library preparation. Genes with an expression above the given quantile "
	                  "are eligible for filtering by the 'pcr_fusions' filter. "
	                  "Default: " + to_string(static_cast<long double>(default_options.high_expression_quantile)))
	     << wrap_help("-e EXONIC_FRACTION", "The breakpoints of false-positive predictions of "
	                  "intragenic events are often both in exons. True predictions are more likely "
	                  "to have at least one breakpoint in an intron, because introns are larger. "
	                  "If the fraction of exonic sequence between two breakpoints is smaller than "
	                  "the given fraction, the 'intragenic_exonic' filter discards the event. "
	                  "Default: " + to_string(static_cast<long double>(default_options.exonic_fraction)))
	     << wrap_help("-T", "When set, the column 'fusion_transcript' is populated with "
	                  "the sequence of the fused genes as assembled from the supporting reads. "
	                  "Specify the flag twice to also print the fusion transcripts to the file "
	                  "containing discarded fusions (-O). Default: " + string((default_options.print_fusion_sequence) ? "on" : "off"))
	     << wrap_help("-P", "When set, the column 'peptide_sequence' is populated with "
	                  "the sequence of the fused proteins as assembled from the supporting reads. "
	                  "Specify the flag twice to also print the peptide sequence to the file "
	                  "containing discarded fusions (-O). Default: " + string((default_options.print_peptide_sequence) ? "on" : "off"))
	     << wrap_help("-I", "When set, the column 'read_identifiers' is populated with "
	                  "identifiers of the reads which support the fusion. The identifiers "
	                  "are separated by commas. Specify the flag twice to also print the read "
	                  "identifiers to the file containing discarded fusions (-O). Default: " + string((default_options.print_supporting_reads) ? "on" : "off"))
	     << wrap_help("-h", "Print help and exit.")
	     << "For more information or help, visit: " << HELP_CONTACT << endl
	     << "The user manual is available at: " << MANUAL_URL << endl;
}

options_t parse_arguments(int argc, char **argv) {
	options_t options = get_default_options();

	// throw error when first argument is not prefixed with a dash
	// for some reason getopt does not detect this error and simply skips the argument
	if (argc > 1 && (string(argv[1]).empty() || argv[1][0] != '-')) {
		cerr << "ERROR: Cannot interpret the first argument \"" << argv[1] << "\"." << endl;
		exit(1);
	}

	// parse arguments
	opterr = 0;
	int c;
	string junction_suffix(".junction");
	unordered_map<char,unsigned int> duplicate_arguments;
	while ((c = getopt(argc, argv, "c:x:d:g:G:o:O:a:b:k:s:i:f:E:S:m:L:H:D:R:A:M:K:V:F:U:Q:e:TPIh")) != -1) {

		// throw error if the same argument is specified more than once
		duplicate_arguments[c]++;
		if (!(duplicate_arguments[c] <= 1 || (c == 'I' || c == 'T' || c == 'P') && duplicate_arguments[c] <= 2)) {
			cerr << "ERROR: Argument -" << ((char) c) << " specified too often" << endl;
			exit(1);
		}

		switch (c) {
			case 'c':
				options.chimeric_bam_file = optarg;
				if (access(options.chimeric_bam_file.c_str(), R_OK) != 0) {
					cerr << "ERROR: File '" << options.chimeric_bam_file << "' not found." << endl;
					exit(1);
				}
				if (options.chimeric_bam_file.size() >= junction_suffix.size() &&
				    options.chimeric_bam_file.substr(options.chimeric_bam_file.size() - junction_suffix.size()) == junction_suffix) {
					cerr << "WARNING: It seems you passed the chimeric junction file ('Chimeric.out.junction') to the parameter -c. However, this parameter takes the chimeric alignments file ('Chimeric.out.sam') as input." << endl;
					exit(1);
				}
				break;
			case 'x': {
				options.rna_bam_file = optarg;
				if (access(options.rna_bam_file.c_str(), R_OK) != 0) {
					cerr << "ERROR: File '" << options.rna_bam_file << "' not found." << endl;
					exit(1);
				}
				break;
			}
			case 'd':
				options.genomic_breakpoints_file = optarg;
				if (access(options.genomic_breakpoints_file.c_str(), R_OK) != 0) {
					cerr << "ERROR: File '" << options.genomic_breakpoints_file << "' not found." << endl;
					exit(1);
				}
				break;
			case 'g':
				options.gene_annotation_file = optarg;
				if (access(options.gene_annotation_file.c_str(), R_OK) != 0) {
					cerr << "ERROR: File '" << options.gene_annotation_file << "' not found." << endl;
					exit(1);
				}
				break;
			case 'G':
				options.gtf_features = optarg;
				{
					gtf_features_t gtf_features;
					if (!parse_gtf_features(options.gtf_features, gtf_features)) {
						cerr << "ERROR: Malformed GTF features: " << options.gtf_features << endl;
						exit(1);
					}
				}
				break;
			case 'o':
				options.output_file = optarg;
				if (!output_directory_exists(options.output_file)) {
					cerr << "ERROR: Parent directory of output file '" << options.output_file << "' does not exist." << endl;
					exit(1);
				}
				break;
			case 'O':
				options.discarded_output_file = optarg;
				if (!output_directory_exists(options.discarded_output_file)) {
					cerr << "ERROR: Parent directory of output file '" << options.discarded_output_file << "' does not exist." << endl;
					exit(1);
				}
				break;
			case 'a':
				options.assembly_file = optarg;
				if (access(options.assembly_file.c_str(), R_OK) != 0) {
					cerr << "ERROR: File '" << options.assembly_file << "' not found." << endl;
					exit(1);
				}
				// when CRAM files are used, the FastA file must be indexed
				if (options.rna_bam_file.size() >= 5 && options.rna_bam_file.substr(options.rna_bam_file.size()-5) == ".cram")
					if (access((options.assembly_file + ".fai").c_str(), R_OK) != 0) {
						cerr << "ERROR: Index for '" << options.assembly_file << "' not found." << endl;
						exit(1);
					}
				break;
			case 'b':
				options.blacklist_file = optarg;
				if (access(options.blacklist_file.c_str(), R_OK) != 0) {
					cerr << "ERROR: File '" << options.blacklist_file << "' not found." << endl;
					exit(1);
				}
				break;
			case 'k':
				options.known_fusions_file = optarg;
				if (access(options.known_fusions_file.c_str(), R_OK) != 0) {
					cerr << "ERROR: File '" << options.known_fusions_file << "' not found." << endl;
					exit(1);
				}
				break;
			case 's':
				if (string(optarg) == "auto") {
					options.strandedness = STRANDEDNESS_AUTO;
				} else if (string(optarg) == "yes") {
					options.strandedness = STRANDEDNESS_YES;
				} else if (string(optarg) == "no") {
					options.strandedness = STRANDEDNESS_NO;
				} else if (string(optarg) == "reverse") {
					options.strandedness = STRANDEDNESS_REVERSE;
				} else {
					cerr << "ERROR: Invalid type of strandedness: " << optarg << endl;
					exit(1);
				}
				break;
			case 'i':
				options.interesting_contigs = optarg;
				replace(options.interesting_contigs.begin(), options.interesting_contigs.end(), ',', ' ');
				break;
			case 'f':
				{
					string disabled_filters;
					disabled_filters = optarg;
					replace(disabled_filters.begin(), disabled_filters.end(), ',', ' ');
					istringstream disabled_filters_ss;
					disabled_filters_ss.str(disabled_filters);
					while (disabled_filters_ss) {
						string disabled_filter;
						disabled_filters_ss >> disabled_filter;
						if (!disabled_filter.empty() && options.filters.find(disabled_filter) == options.filters.end()) {
							cerr << "ERROR: Invalid argument to option -f: " << disabled_filter << endl;
							exit(1);
						} else
							options.filters[disabled_filter] = false;
					}
				}
				break;
			case 'E':
				if (!validate_float(optarg, options.evalue_cutoff, 0)) {
					cerr << "ERROR: " << "Argument to -" << ((char) c) << " must be greater than 0." << endl;
					exit(1);
				}
				break;
			case 'S':
				if (!validate_int(optarg, options.min_support, 0)) {
					cerr << "ERROR: " << "Invalid argument to -" << ((char) c) << "." << endl;
					exit(1);
				}
				break;
			case 'm':
				if (!validate_float(optarg, options.max_mismapper_fraction, 0, 1)) {
					cerr << "ERROR: " << "Argument to -" << ((char) c) << " must be between 0 and 1." << endl;
					exit(1);
				}
				break;
			case 'L':
				if (!validate_float(optarg, options.max_homolog_identity, 0, 1)) {
					cerr << "ERROR: " << "Argument to -" << ((char) c) << " must be between 0 and 1." << endl;
					exit(1);
				}
				break;
			case 'H':
				if (!validate_int(optarg, options.homopolymer_length, 2)) {
					cerr << "ERROR: " << "Argument to -" << ((char) c) << " must be greater than 1." << endl;
					exit(1);
				}
				break;
			case 'D':
				if (!validate_int(optarg, options.max_genomic_breakpoint_distance, 0)) {
					cerr << "ERROR: " << "Invalid argument to -" << ((char) c) << "." << endl;
					exit(1);
				}
				break;
			case 'R':
				if (!validate_int(optarg, options.min_read_through_distance, 0)) {
					cerr << "ERROR: " << "Invalid argument to -" << ((char) c) << "." << endl;
					exit(1);
				}
				break;
			case 'A':
				if (!validate_int(optarg, options.min_anchor_length, 0)) {
					cerr << "ERROR: " << "Invalid argument to -" << ((char) c) << "." << endl;
					exit(1);
				}
				break;
			case 'M':
				if (!validate_int(optarg, options.min_spliced_events, 0)) {
					cerr << "ERROR: " << "Invalid argument to -" << ((char) c) << "." << endl;
					exit(1);
				}
				break;
			case 'K':
				if (!validate_float(optarg, options.max_kmer_content, 0, 1)) {
					cerr << "ERROR: " << "Argument to -" << ((char) c) << " must be between 0 and 1." << endl;
					exit(1);
				}
				break;
			case 'V':
				if (!validate_float(optarg, options.mismatch_pvalue_cutoff, 0, 1)) {
					cerr << "ERROR: " << "Argument to -" << ((char) c) << " must be between 0 and 1." << endl;
					exit(1);
				}
				break;
			case 'F':
				if (!validate_int(optarg, options.fragment_length, 1)) {
					cerr << "ERROR: " << "Argument to -" << ((char) c) << " must be an integer greater than 0." << endl;
					exit(1);
				}
				break;
			case 'U':
				if (!validate_int(optarg, options.subsampling_threshold, 1, SHRT_MAX)) {
					cerr << "ERROR: " << "Argument to -" << ((char) c) << " must be an integer between 1 and " << SHRT_MAX << "." << endl;
					exit(1);
				}
				break;
			case 'Q':
				if (!validate_float(optarg, options.high_expression_quantile, 0, 1)) {
					cerr << "ERROR: " << "Argument to -" << ((char) c) << " must be between 0 and 1." << endl;
					exit(1);
				}
				break;
			case 'e':
				if (!validate_float(optarg, options.exonic_fraction, 0, 1)) {
					cerr << "ERROR: " << "Argument to -" << ((char) c) << " must be between 0 and 1." << endl;
					exit(1);
				}
				break;
			case 'T':
				if (!options.print_fusion_sequence)
					options.print_fusion_sequence = true;
				else
					options.print_fusion_sequence_for_discarded_fusions = true;
				break;
			case 'P':
				if (!options.print_peptide_sequence)
					options.print_peptide_sequence = true;
				else
					options.print_peptide_sequence_for_discarded_fusions = true;
				break;
			case 'I':
				if (!options.print_supporting_reads)
					options.print_supporting_reads = true;
				else
					options.print_supporting_reads_for_discarded_fusions = true;
				break;
			case 'h':
				print_usage();
				exit(0);
				break;
			default:
				switch (optopt) {
					case 'c': case 'x': case 'd': case 'g': case 'G': case 'o': case 'O': case 'a': case 'k': case 'b': case 'i': case 'f': case 'E': case 's': case 'm': case 'H': case 'D': case 'R': case 'A': case 'M': case 'K': case 'V': case 'F': case 'S': case 'U': case 'Q':
						cerr << "ERROR: " << "Option -" << ((char) optopt) << " requires an argument." << endl;
						exit(1);
						break;
					default:
						cerr << "ERROR: " << "Unknown option: -" << ((char) optopt) << endl;
						exit(1);
						break;
				}
		}

		if (optind < argc && (string(argv[optind]).empty() || argv[optind][0] != '-')) {
			cerr << "ERROR: Option -" << ((char) c) << " has more than one argument. Arguments with blanks must be wrapped in quotes." << endl;
			exit(1);
		}

	}

	// check for mandatory arguments
	if (argc == 1) {
		cerr << "ERROR: No arguments given." << endl;
		print_usage();
		exit(1);
	}
	if (options.rna_bam_file.empty()) {
		cerr << "ERROR: Missing mandatory option: -x" << endl;
		exit(1);
	}
	if (options.gene_annotation_file.empty()) {
		cerr << "ERROR: Missing mandatory option: -g" << endl;
		exit(1);
	}
	if (options.output_file.empty()) {
		cerr << "ERROR: Missing mandatory option: -o" << endl;
		exit(1);
	}
	if (options.assembly_file.empty()) {
        	cerr << "ERROR: Missing mandatory option: -a" << endl;
		exit(1);
	}
	if (options.filters["blacklist"] && options.blacklist_file.empty()) {
		cerr << "ERROR: Filter 'blacklist' enabled, but missing option: -b" << endl;
		exit(1);
	}

	return options;
}

