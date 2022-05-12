#include "gtest/gtest.h"
#include <gmp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>		// mkstemp

extern "C" {
#include "snp4.h"		/* API */
}

class SNP4P4BMFileTest : public ::testing::Test {
protected:
  SNP4P4BMFileTest() {
    // Set up a temp file
    char fname[] = "/tmp/snp4_gtest_XXXXXX";
    int fd = mkstemp(fname);
    f = fdopen(fd, "r+");
    remove(fname);
  }

  void TearDown() override {
    if (f) fclose(f);
  }

  FILE * f = NULL;
};

TEST_F(SNP4P4BMFileTest, ParseAddKeyOnly) {
  // Set up content in the temp file
  const char s[] =
    "table_add t_p128 a_nop 0x0102030405060708090a0b0c0d0e0f =>\n";
  fprintf(f, s);
  rewind(f);

  struct sn_cfg_set * cfg_set = snp4_cfg_set_load_p4bm(f);

  ASSERT_NE(nullptr, cfg_set);
  ASSERT_EQ(1, cfg_set->num_entries);

  struct sn_cfg * cfg = cfg_set->entries[0];
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_p128",  cfg->rule.table_name);
  ASSERT_EQ(1, cfg->rule.num_matches);
  ASSERT_STREQ("a_nop", cfg->rule.action_name);
  ASSERT_EQ(0, cfg->rule.num_params);
  ASSERT_EQ(0, cfg->rule.priority);

  struct sn_match * m = &cfg->rule.matches[0];
  ASSERT_EQ(SN_MATCH_FORMAT_KEY_ONLY, m->t);
  mpz_t key;
  mpz_init_set_str(key, "0x0102030405060708090a0b0c0d0e0f", 0);
  ASSERT_EQ(0, mpz_cmp(key, m->v.key_only.key));
}

TEST_F(SNP4P4BMFileTest, ParseAddKeyMask) {
  // Set up content in the temp file
  const char s[] =
    "table_add t_p128 a_nop 0xabc&&&0xff0 =>\n";
  fprintf(f, s);
  rewind(f);

  struct sn_cfg_set * cfg_set = snp4_cfg_set_load_p4bm(f);

  ASSERT_NE(nullptr, cfg_set);
  ASSERT_EQ(1, cfg_set->num_entries);

  struct sn_cfg * cfg = cfg_set->entries[0];
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_p128",  cfg->rule.table_name);
  ASSERT_EQ(1, cfg->rule.num_matches);
  ASSERT_STREQ("a_nop", cfg->rule.action_name);
  ASSERT_EQ(0, cfg->rule.num_params);
  ASSERT_EQ(0, cfg->rule.priority);

  struct sn_match * m = &cfg->rule.matches[0];
  ASSERT_EQ(SN_MATCH_FORMAT_KEY_MASK, m->t);
  mpz_t key;
  mpz_init_set_str(key, "0xabc", 0);
  ASSERT_EQ(0, mpz_cmp(key, m->v.key_mask.key));

  mpz_t mask;
  mpz_init_set_str(mask, "0xff0", 0);
  ASSERT_EQ(0, mpz_cmp(mask, m->v.key_mask.mask));
}

TEST_F(SNP4P4BMFileTest, ParseAddRangeHexHex) {
  // Set up content in the temp file
  const char s[] =
    "table_add t_p128 a_nop 0xabc->0xff0 =>\n";
  fprintf(f, s);
  rewind(f);

  struct sn_cfg_set * cfg_set = snp4_cfg_set_load_p4bm(f);

  ASSERT_NE(nullptr, cfg_set);
  ASSERT_EQ(1, cfg_set->num_entries);

  struct sn_cfg * cfg = cfg_set->entries[0];
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_p128",  cfg->rule.table_name);
  ASSERT_EQ(1, cfg->rule.num_matches);
  ASSERT_STREQ("a_nop", cfg->rule.action_name);
  ASSERT_EQ(0, cfg->rule.num_params);
  ASSERT_EQ(0, cfg->rule.priority);

  struct sn_match * m = &cfg->rule.matches[0];
  ASSERT_EQ(SN_MATCH_FORMAT_RANGE, m->t);
  ASSERT_EQ(0xabc, m->v.range.lower);
  ASSERT_EQ(0xff0, m->v.range.upper);
}

TEST_F(SNP4P4BMFileTest, ParseAddRangeDecDec) {
  // Set up content in the temp file
  const char s[] =
    "table_add t_p128 a_nop 33->99 =>\n";
  fprintf(f, s);
  rewind(f);

  struct sn_cfg_set * cfg_set = snp4_cfg_set_load_p4bm(f);

  ASSERT_NE(nullptr, cfg_set);
  ASSERT_EQ(1, cfg_set->num_entries);

  struct sn_cfg * cfg = cfg_set->entries[0];
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_p128",  cfg->rule.table_name);
  ASSERT_EQ(1, cfg->rule.num_matches);
  ASSERT_STREQ("a_nop", cfg->rule.action_name);
  ASSERT_EQ(0, cfg->rule.num_params);
  ASSERT_EQ(0, cfg->rule.priority);

  struct sn_match * m = &cfg->rule.matches[0];
  ASSERT_EQ(SN_MATCH_FORMAT_RANGE, m->t);
  ASSERT_EQ(33, m->v.range.lower);
  ASSERT_EQ(99, m->v.range.upper);
}

TEST_F(SNP4P4BMFileTest, ParseAddRangeBinBin) {
  // Set up content in the temp file
  const char s[] =
    "table_add t_p128 a_nop 0b1011->0b1100 =>\n";
  fprintf(f, s);
  rewind(f);

  struct sn_cfg_set * cfg_set = snp4_cfg_set_load_p4bm(f);

  ASSERT_NE(nullptr, cfg_set);
  ASSERT_EQ(1, cfg_set->num_entries);

  struct sn_cfg * cfg = cfg_set->entries[0];
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_p128",  cfg->rule.table_name);
  ASSERT_EQ(1, cfg->rule.num_matches);
  ASSERT_STREQ("a_nop", cfg->rule.action_name);
  ASSERT_EQ(0, cfg->rule.num_params);
  ASSERT_EQ(0, cfg->rule.priority);

  struct sn_match * m = &cfg->rule.matches[0];
  ASSERT_EQ(SN_MATCH_FORMAT_RANGE, m->t);
  ASSERT_EQ(11, m->v.range.lower);
  ASSERT_EQ(12, m->v.range.upper);
}

TEST_F(SNP4P4BMFileTest, ParseAddPrefixHexHex) {
  // Set up content in the temp file
  const char s[] =
    "table_add t_p128 a_nop 0xabc/0xa =>\n";
  fprintf(f, s);
  rewind(f);

  struct sn_cfg_set * cfg_set = snp4_cfg_set_load_p4bm(f);

  ASSERT_NE(nullptr, cfg_set);
  ASSERT_EQ(1, cfg_set->num_entries);

  struct sn_cfg * cfg = cfg_set->entries[0];
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_p128",  cfg->rule.table_name);
  ASSERT_EQ(1, cfg->rule.num_matches);
  ASSERT_STREQ("a_nop", cfg->rule.action_name);
  ASSERT_EQ(0, cfg->rule.num_params);
  ASSERT_EQ(0, cfg->rule.priority);

  struct sn_match * m = &cfg->rule.matches[0];
  ASSERT_EQ(SN_MATCH_FORMAT_PREFIX, m->t);
  mpz_t key;
  mpz_init_set_str(key, "0xabc", 0);
  ASSERT_EQ(0, mpz_cmp(key, m->v.prefix.key));
  ASSERT_EQ(0xa, m->v.prefix.prefix_len);
}

TEST_F(SNP4P4BMFileTest, ParseAddPrefixDecDec) {
  // Set up content in the temp file
  const char s[] =
    "table_add t_p128 a_nop 999/33 =>\n";
  fprintf(f, s);
  rewind(f);

  struct sn_cfg_set * cfg_set = snp4_cfg_set_load_p4bm(f);

  ASSERT_NE(nullptr, cfg_set);
  ASSERT_EQ(1, cfg_set->num_entries);

  struct sn_cfg * cfg = cfg_set->entries[0];
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_p128",  cfg->rule.table_name);
  ASSERT_EQ(1, cfg->rule.num_matches);
  ASSERT_STREQ("a_nop", cfg->rule.action_name);
  ASSERT_EQ(0, cfg->rule.num_params);
  ASSERT_EQ(0, cfg->rule.priority);

  struct sn_match * m = &cfg->rule.matches[0];
  ASSERT_EQ(SN_MATCH_FORMAT_PREFIX, m->t);
  mpz_t key;
  mpz_init_set_str(key, "999", 0);
  ASSERT_EQ(0, mpz_cmp(key, m->v.prefix.key));
  ASSERT_EQ(33, m->v.prefix.prefix_len);
}

TEST_F(SNP4P4BMFileTest, ParseAddPrefixBinBin) {
  // Set up content in the temp file
  const char s[] =
    "table_add t_p128 a_nop 0b10111/0b11100 =>\n";
  fprintf(f, s);
  rewind(f);

  struct sn_cfg_set * cfg_set = snp4_cfg_set_load_p4bm(f);

  ASSERT_NE(nullptr, cfg_set);
  ASSERT_EQ(1, cfg_set->num_entries);

  struct sn_cfg * cfg = cfg_set->entries[0];
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_p128",  cfg->rule.table_name);
  ASSERT_EQ(1, cfg->rule.num_matches);
  ASSERT_STREQ("a_nop", cfg->rule.action_name);
  ASSERT_EQ(0, cfg->rule.num_params);
  ASSERT_EQ(0, cfg->rule.priority);

  struct sn_match * m = &cfg->rule.matches[0];
  ASSERT_EQ(SN_MATCH_FORMAT_PREFIX, m->t);
  mpz_t key;
  mpz_init_set_str(key, "0b10111", 0);
  ASSERT_EQ(0, mpz_cmp(key, m->v.prefix.key));
  ASSERT_EQ(28, m->v.prefix.prefix_len);
}

TEST_F(SNP4P4BMFileTest, ParseAddTp128AnopNoPrio) {
  // Set up content in the temp file
  const char s[] =
    "table_add t_p128 a_nop 0xabc =>\n";
  fprintf(f, s);
  rewind(f);

  struct sn_cfg_set * cfg_set = snp4_cfg_set_load_p4bm(f);

  ASSERT_NE(nullptr, cfg_set);
  ASSERT_EQ(1, cfg_set->num_entries);

  struct sn_cfg * cfg = cfg_set->entries[0];
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_p128",  cfg->rule.table_name);
  ASSERT_EQ(1, cfg->rule.num_matches);
  ASSERT_STREQ("a_nop", cfg->rule.action_name);
  ASSERT_EQ(0, cfg->rule.num_params);
  ASSERT_EQ(0, cfg->rule.priority);
}

TEST_F(SNP4P4BMFileTest, ParseAddTp128AnopWithPrio) {
  // Set up content in the temp file
  const char s[] =
    "table_add t_p128 a_nop 0xabc => 3\n";
  fprintf(f, s);
  rewind(f);

  struct sn_cfg_set * cfg_set = snp4_cfg_set_load_p4bm(f);

  ASSERT_NE(nullptr, cfg_set);
  ASSERT_EQ(1, cfg_set->num_entries);

  struct sn_cfg * cfg = cfg_set->entries[0];
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_p128",  cfg->rule.table_name);
  ASSERT_EQ(1, cfg->rule.num_matches);
  ASSERT_STREQ("a_nop", cfg->rule.action_name);
  ASSERT_EQ(0, cfg->rule.num_params);
  ASSERT_EQ(3, cfg->rule.priority);
}

TEST_F(SNP4P4BMFileTest, ParseAddTmultiAthreeNoPrio) {
  // Set up content in the temp file
  const char s[] =
    "table_add t_multi a_three 33 9 0xa&&&3 0b111->0b10000 19/4 1 => 1 2 3\n";
  fprintf(f, s);
  rewind(f);

  struct sn_cfg_set * cfg_set = snp4_cfg_set_load_p4bm(f);

  ASSERT_NE(nullptr, cfg_set);
  ASSERT_EQ(1, cfg_set->num_entries);

  struct sn_cfg * cfg = cfg_set->entries[0];
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_multi",  cfg->rule.table_name);
  ASSERT_EQ(6, cfg->rule.num_matches);
  ASSERT_STREQ("a_three", cfg->rule.action_name);
  ASSERT_EQ(3, cfg->rule.num_params);
  ASSERT_EQ(0, cfg->rule.priority);

  mpz_t key;
  mpz_t mask;
  struct sn_match * m;
  mpz_t param;
  struct sn_param * p;

  // Match 0
  m = &cfg->rule.matches[0];
  ASSERT_EQ(SN_MATCH_FORMAT_KEY_ONLY, m->t);
  mpz_init_set_str(key, "33", 0);
  ASSERT_EQ(0, mpz_cmp(key, m->v.key_only.key));

  // Match 1
  m = &cfg->rule.matches[1];
  ASSERT_EQ(SN_MATCH_FORMAT_KEY_ONLY, m->t);
  mpz_init_set_str(key, "9", 0);
  ASSERT_EQ(0, mpz_cmp(key, m->v.key_only.key));

  // Match 2
  m = &cfg->rule.matches[2];
  ASSERT_EQ(SN_MATCH_FORMAT_KEY_MASK, m->t);
  mpz_init_set_str(key,  "0xa", 0);
  mpz_init_set_str(mask, "3", 0);
  ASSERT_EQ(0, mpz_cmp(key,  m->v.key_mask.key));
  ASSERT_EQ(0, mpz_cmp(mask, m->v.key_mask.mask));

  // Match 3
  m = &cfg->rule.matches[3];
  ASSERT_EQ(SN_MATCH_FORMAT_RANGE, m->t);
  ASSERT_EQ(7, m->v.range.lower);
  ASSERT_EQ(16, m->v.range.upper);

  // Match 4
  m = &cfg->rule.matches[4];
  ASSERT_EQ(SN_MATCH_FORMAT_PREFIX, m->t);
  mpz_init_set_str(key, "19", 0);
  ASSERT_EQ(0, mpz_cmp(key, m->v.prefix.key));
  ASSERT_EQ(4, m->v.prefix.prefix_len);

  // Match 5
  m = &cfg->rule.matches[5];
  ASSERT_EQ(SN_MATCH_FORMAT_KEY_ONLY, m->t);
  mpz_init_set_str(key, "1", 0);
  ASSERT_EQ(0, mpz_cmp(key, m->v.key_only.key));

  // Param 0
  p = &cfg->rule.params[0];
  ASSERT_EQ(SN_PARAM_FORMAT_MPZ, p->t);
  mpz_init_set_str(param, "1", 0);
  ASSERT_EQ(0, mpz_cmp(param, p->v.mpz));

  // Param 1
  p = &cfg->rule.params[1];
  ASSERT_EQ(SN_PARAM_FORMAT_MPZ, p->t);
  mpz_init_set_str(param, "2", 0);
  ASSERT_EQ(0, mpz_cmp(param, p->v.mpz));

  // Param 2
  p = &cfg->rule.params[2];
  ASSERT_EQ(SN_PARAM_FORMAT_MPZ, p->t);
  mpz_init_set_str(param, "3", 0);
  ASSERT_EQ(0, mpz_cmp(param, p->v.mpz));
}

TEST_F(SNP4P4BMFileTest, ParseAddMultiLine) {
  // Set up content in the temp file
  const char s[] =
    "table_add t_multi a_three 33 9 0xa&&&3 0b111->0b10000 19/4 1 => 1 2 3\n"
    "table_add t_multi a_one   1 2 3 4 5 6 => 1\n"
    "# this is a comment line\n"
    "\n"
    "table_add t_p128 a_nop    0x9999 =>3\n"
    "# another comment line at the end\n";
  fprintf(f, s);
  rewind(f);

  struct sn_cfg_set * cfg_set = snp4_cfg_set_load_p4bm(f);

  ASSERT_NE(nullptr, cfg_set);
  ASSERT_EQ(3, cfg_set->num_entries);

  struct sn_cfg * cfg;

  // Entry 0
  cfg = cfg_set->entries[0];
  ASSERT_EQ(1, cfg->line_no);
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_multi",  cfg->rule.table_name);
  ASSERT_EQ(6, cfg->rule.num_matches);
  ASSERT_STREQ("a_three", cfg->rule.action_name);
  ASSERT_EQ(3, cfg->rule.num_params);
  ASSERT_EQ(0, cfg->rule.priority);

  // Entry 1
  cfg = cfg_set->entries[1];
  ASSERT_EQ(2, cfg->line_no);
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_multi",  cfg->rule.table_name);
  ASSERT_EQ(6, cfg->rule.num_matches);
  ASSERT_STREQ("a_one", cfg->rule.action_name);
  ASSERT_EQ(1, cfg->rule.num_params);
  ASSERT_EQ(0, cfg->rule.priority);

  // Entry 2
  cfg = cfg_set->entries[2];
  ASSERT_EQ(5, cfg->line_no);
  ASSERT_EQ(OP_TABLE_ADD, cfg->op);
  ASSERT_STREQ("t_p128",  cfg->rule.table_name);
  ASSERT_EQ(1, cfg->rule.num_matches);
  ASSERT_STREQ("a_nop", cfg->rule.action_name);
  ASSERT_EQ(0, cfg->rule.num_params);
  ASSERT_EQ(3, cfg->rule.priority);
}
