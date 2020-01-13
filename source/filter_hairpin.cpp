#include "sam.h"
#include "common.hpp"
#include "annotation.hpp"
#include "filter_hairpin.hpp"

using namespace std;

bool is_breakpoint_within_aligned_segment(const position_t breakpoint, const alignment_t& alignment) {
	// find out where the read aligns and check if the breakpoint is within this aligned section
	position_t reference_position = alignment.start;
	for (unsigned int cigar_element = 0; cigar_element < alignment.cigar.size(); ++cigar_element) {
		switch (alignment.cigar.operation(cigar_element)) {
			case BAM_CREF_SKIP:
			case BAM_CDEL:
				reference_position += alignment.cigar.op_length(cigar_element);
				break;
			case BAM_CMATCH:
			case BAM_CDIFF:
			case BAM_CEQUAL:
				if (breakpoint >= reference_position && breakpoint <= reference_position + (int) alignment.cigar.op_length(cigar_element))
					return true;
				reference_position += alignment.cigar.op_length(cigar_element);
				break;
		}
	}
	return false;
}

unsigned int filter_hairpin(chimeric_alignments_t& chimeric_alignments, exon_annotation_index_t& exon_annotation_index, const int max_mate_gap) {

	unsigned int remaining = 0;
	for (chimeric_alignments_t::iterator chimeric_alignment = chimeric_alignments.begin(); chimeric_alignment != chimeric_alignments.end(); ++chimeric_alignment) {

		if (chimeric_alignment->second.filter != FILTER_none)
			continue; // the read has already been filtered

		// check if mate1 and mate2 map to the same gene or close to one another
		gene_set_t common_genes;
		if (chimeric_alignment->second.size() == 2) { // discordant mate
			combine_annotations(chimeric_alignment->second[MATE1].genes, chimeric_alignment->second[MATE2].genes, common_genes, false);
			if (common_genes.empty() && chimeric_alignment->second[MATE1].contig != chimeric_alignment->second[MATE2].contig) {
				remaining++;
				continue; // we are only interested in intragenic events
			}
		} else {// split read
			combine_annotations(chimeric_alignment->second[SPLIT_READ].genes, chimeric_alignment->second[SUPPLEMENTARY].genes, common_genes, false);
			if (common_genes.empty() && chimeric_alignment->second[SPLIT_READ].contig != chimeric_alignment->second[SUPPLEMENTARY].contig) {
				remaining++;
				continue; // we are only interested in intragenic events
			}
		}

		if (chimeric_alignment->second.size() == 2) { // discordant mates

			position_t breakpoint1 = (chimeric_alignment->second[MATE1].strand == FORWARD) ? chimeric_alignment->second[MATE1].end : chimeric_alignment->second[MATE1].start;
			position_t breakpoint2 = (chimeric_alignment->second[MATE2].strand == FORWARD) ? chimeric_alignment->second[MATE2].end : chimeric_alignment->second[MATE2].start;

			if (is_breakpoint_within_aligned_segment(breakpoint1, chimeric_alignment->second[MATE2]) ||
			    is_breakpoint_within_aligned_segment(breakpoint2, chimeric_alignment->second[MATE1])) {
				chimeric_alignment->second.filter = FILTER_hairpin;
				continue;
			}

		} else { // split read

			position_t breakpoint_split_read = (chimeric_alignment->second[SPLIT_READ].strand == FORWARD) ? chimeric_alignment->second[SPLIT_READ].start : chimeric_alignment->second[SPLIT_READ].end;
			position_t breakpoint_supplementary = (chimeric_alignment->second[SUPPLEMENTARY].strand == FORWARD) ? chimeric_alignment->second[SUPPLEMENTARY].end : chimeric_alignment->second[SUPPLEMENTARY].start;
			if (is_breakpoint_within_aligned_segment(breakpoint_split_read, chimeric_alignment->second[SUPPLEMENTARY]) ||
			    is_breakpoint_within_aligned_segment(breakpoint_supplementary, chimeric_alignment->second[SPLIT_READ]) ||
			    is_breakpoint_within_aligned_segment(breakpoint_supplementary, chimeric_alignment->second[MATE1])) {
				chimeric_alignment->second.filter = FILTER_hairpin;
				continue;
			}

		}

		remaining++;
	}
	return remaining;
}

