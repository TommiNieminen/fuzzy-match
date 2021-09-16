#include <fuzzy/ngram_matches.hh>

#include <cmath>

namespace fuzzy
{
  unsigned compute_min_exact_match(float fuzzy, unsigned p_length)
  {
    const auto differences = (unsigned)std::ceil(p_length * (1.f - fuzzy));
    // we split (p_length - differences) in  (differences + 1) parts
    // the minimum value of the largest part size is obtained by dividing and taking ceil
    return std::ceil((p_length - differences) / (differences + 1.));
  }

  NGramMatches::NGramMatches(size_t size_tm,
                             float fuzzy, unsigned p_length,
                             unsigned min_seq_len,
                             const SuffixArray& suffixArray)
    /* add a small epsilon to avoid rounding errors counting for an error */
    : max_differences_with_pattern((unsigned)std::floor(p_length * (1.f - fuzzy) + 0.00005)),
      min_exact_match(compute_min_exact_match(fuzzy, p_length)),
      _p_length(p_length),
      _min_seq_len(min_seq_len),
      _suffixArray(suffixArray)
  {
    _psentences.reserve(p_length);
  }

  int
  NGramMatches::get_sentence_count() const
  {
    return _psentences.size();
  }

  tsl::hopscotch_map<unsigned, AgendaItem>&
  NGramMatches::get_psentences()
  {
    return _psentences;
  }

  void
  NGramMatches::register_suffix_range(size_t begin, size_t end, size_t match_length)
  {
    // lazy injection feature - if match_length smaller than min_seq_len, we will not process the suffixes for the moment
    if (match_length < min_exact_match || match_length < _min_seq_len)
      return;

    // For each suffix that matches at least match_length
    for (auto i = begin; i < end; i++)
    {
      // The size difference between the suffix and the pattern is too large for the suffix to be accepted
      const auto sizeDifference = std::abs((long int)_p_length - (long int)_suffixArray.sentence_length(i));
      if (sizeDifference > max_differences_with_pattern)
        continue;

      // Get or create the AgendaItem corresponding to the sentence (of the suffix that matched)
      const auto sentence_id = _suffixArray.suffixid2sentenceid()[i].sentence_id;
      auto& agendaItem = _psentences.try_emplace(sentence_id, sentence_id, _p_length).first.value();

      // The match will update the AgendaItem entry only if its length is the longest to date.
      if (match_length > agendaItem.maxmatch)
      {
        for (size_t j = agendaItem.maxmatch; j < match_length; j++)
        {
          if (!agendaItem.map_pattern[j])
          {
            agendaItem.map_pattern[j] = true;
            agendaItem.coverage++;
          }
        }

        agendaItem.maxmatch = match_length;
      }
    }
  }
}
