#include <fuzzy/bm25.hh>

#include <fuzzy/ngram_matches.hh>
#include <fuzzy/vocab_indexer.hh>
#include <cassert>

namespace fuzzy
{
  BM25::BM25(float k1, float b)
    : _k1(k1), _b(b)
  {}
  BM25::~BM25()
  {
    if (_bm25 != nullptr)
      delete _bm25;
  }
  unsigned
  BM25::add_sentence(const std::vector<unsigned>& sentence)
  {
    size_t sidx = _sentence_pos.size();
    _sentence_pos.push_back(_sentence_buffer.size());

    /* first token in sentence buffer is the sentence size */
    _sentence_buffer.push_back(sentence.size());

    for (size_t i = 0; i < sentence.size(); i++)
    {
      _sentence_buffer.push_back(sentence[i]);
    }
    _sentence_buffer.push_back(fuzzy::VocabIndexer::SENTENCE_SEPARATOR);
    _sorted = false;

    return sidx;
  }

  double BM25::bm25_score_pattern(
    unsigned s_id,
    std::vector<unsigned> pattern_wids) const
  {
    double score = 0;
    for (unsigned& term : pattern_wids){
      score += (*_bm25)[term][s_id];
      std::cerr << term << ":" << (*_bm25)[term][s_id] << " | ";
    }
    std::cerr << std::endl;
    return score;
  }

  double BM25::bm25_score(
    unsigned term,
    unsigned s_id,
    double avg_doc_length,
    boost::multi_array<unsigned, 2>& tf,
    std::vector<float>& idf)
  {
    /* BM25 formula */
    std::cerr << idf[term] * (_k1 + 1) * tf[term][s_id] / (tf[term][s_id] + _k1 * ((1 - _b) + (_b * (get_sentence_length(s_id) / avg_doc_length)))) << " ";

    return idf[term] * (_k1 + 1) * tf[term][s_id] / 
          (tf[term][s_id] + _k1 * ((1 - _b) + (_b * (get_sentence_length(s_id) / avg_doc_length))));
  }

  void BM25::sort(size_t vocab_size) {
    if (_sorted)
      return;

    std::cerr << "build TF + DF..." << std::endl;
    // build TF + DF
    boost::multi_array<unsigned, 2> tf(boost::extents[vocab_size][_sentence_pos.size()]);
    std::fill(tf.data(), tf.data() + tf.num_elements(), 0);
    std::vector<unsigned> doc_freq(vocab_size, 0);
    std::unordered_set<unsigned> seen_in_doc;
    unsigned sentence_idx = 0;
    unsigned relative_pos = 1;
    for (unsigned pos = 1; pos < _sentence_buffer.size(); pos++, relative_pos++)
    {
      if (sentence_idx + 1 < _sentence_pos.size() && _sentence_pos[sentence_idx + 1] == pos)
      {
        relative_pos = 0;
        sentence_idx++;
        seen_in_doc.clear();
        std::cerr << "| ";
      }
      else if (_sentence_buffer[pos] != 0)
      {
        tf[_sentence_buffer[pos]][sentence_idx]++; // term frequency count
        auto result_insert = seen_in_doc.insert(_sentence_buffer[pos]);
        // std::cerr << "(" << result_insert.second << ") ";
        if (result_insert.second)
          doc_freq[_sentence_buffer[pos]]++;
        std::cerr << _sentence_buffer[pos] << " ";
      }
    }
    std::cerr << std::endl;
    for (auto &e : doc_freq)
      std::cerr << e << " ";
    std::cerr << std::endl;
    std::cerr << "build IDF from DF..." << std::endl;
    // build IDF from DF
    std::vector<float> idf(vocab_size);
    for (unsigned term = 0; term < vocab_size; term++)
    {
      idf[term] = std::log(
        ((double)_sentence_pos.size() - (double)doc_freq[term] + 0.5) /
        ((double)doc_freq[term] + 0.5)
      );
    }
    std::cerr << "build BM25 from TF and IDF..." << std::endl;
    // build BM25 from TF and IDF
    double avg_doc_length = (double)_sentence_buffer.size() / (double)_sentence_pos.size() - 2;
    std::cerr << vocab_size << ", " << _sentence_pos.size() << std::endl;
    _bm25 = new boost::multi_array<float, 2>(boost::extents[vocab_size][_sentence_pos.size()]);
    for (unsigned s_id = 0; s_id < _sentence_pos.size(); s_id++)
      for (unsigned term = 0; term < vocab_size; term++)
        (*_bm25)[term][s_id] = bm25_score(term, s_id, avg_doc_length, tf, idf);

    std::cerr << "sorted" << std::endl;
    _sorted = true;
  }
}
