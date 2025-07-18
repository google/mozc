# Copyright 2010-2021, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

def mozc_dataset(
        name,
        outs,
        a11y_description_src,
        boundary_def,
        cforms,
        collocation_src,
        collocation_suppression_src,
        connection_single_column_src,
        dictionary_srcs,
        emoji_src,
        emoticon_categorized_src,
        emoticon_src,
        id_def,
        magic,
        pos_group_def,
        pos_matcher_rule_def,
        reading_correction_src,
        segmenter_def,
        segmenter_generator_srcs,
        single_kanji_src,
        sorting_map,
        special_pos,
        suffix,
        suggestion_filter_src,
        symbol_ordering_rule,
        symbol_src,
        tag,
        use_1byte_cost,
        user_pos_def,
        variant_rule,
        varname,
        zero_query_def,
        zero_query_number_def,
        suggestion_filter_safe_def_srcs = [],
        usage_dict = None,
        extra_data = []):
    """Macro for Mozc data set.

    This macro defines a set of genrules each of which has name "name + @xxx",
    where xxx is listed below:
      - No suffix (i.e., "name" is target name): Main binary file of the data
      - header: Embedded file (to be included in C++) of the data set file
      - a11y_description: [Optional] Accessibility description data. Available only if
              a11y_description_src is provided.
      - collocation: Collocation data
      - collocation_suppression: Collocation suppression data
      - connection: Connection matrix data
      - dictionary: System dictionary data
      - suggestion_filter: Suggestion filter data
      - pos_group: POS group data
      - boundary: Boundary data
      - segmenter_inl_header: C++ header to be included in segmenter_generator.
      - segmenter_generator: C++ binary generating segmenter data files.
      - segmenter: Data files for segmenter.
      - counter_suffix: Counter suffix sorted array.
      - suffix: Suffix dictionary data.
      - reading_correction: Reading correction arrays.
      - symbol: Symbol dictionary data.
      - usage: [Optional] Usage dictionary data.  Available only if usage_dict is
               provided.
      - user_pos: User POS data.
      - user_pos_manager_data: User POS manager data.
      - pos_matcher: POS matcher data.
      - emoticon_src: Emoticon data
      - emoticon_categorized_src: Categorized emoticon data
      - single_kanji: Single Kanji dictionary and variant data
      - single_kanji_noun_prefix: Single Kanji noun prefix data
      - zero_query_def: Zero query definition file
      - zero_query_number_def: Zero query number definition file
    For usage, see //data_manager/google/BUILD.

    Args:
      name: data set target name to be generated.
      outs: list of output files (mozc.data, mozc_data.inc, and pos_list.inc).
      a11y_description_src: the source of a11y descriptions.
      boundary_def: boundary definition file.
      cforms: cform definition file.
      collocation_src: collocation data file.
      collocation_suppression_src: collocation suppression data file.
      connection_single_column_src: Connection data in text format
      dictionary_srcs: a list of dictionary files.
      emoji_src: emoji data file.
      emoticon_categorized_src: categorized emoji data file.
      emoticon_src: emoticon data file.
      id_def: POS ID definition file.
      magic: the magic number to be embedded in the output data file.
      pos_group_def: POS group definition file.
      pos_matcher_rule_def: POS matcher rule definition file.
      reading_correction_src: reading correction source file.
      segmenter_def: segmenter definition file.
      segmenter_generator_srcs: C++ sources for segmenter code generator.
      single_kanji_src: single Kanji definition file.
      sorting_map: sorting amp file.
      special_pos: special POS definition file.
      suffix: suffix dictionary file.
      suggestion_filter_src: suggestion filter data file.
      symbol_ordering_rule: symbol order definition file.
      symbol_src: symbol data file.
      tag: the tag to be embedded in data version.
      use_1byte_cost: if true, compress connection matrix by using 1 byte costs.
      user_pos_def: user POS file.
      variant_rule: single Kanji variant rule file.
      varname: C++ variable name for the embedded data.
      zero_query_def: rule-based zero query suggestion data file.
      zero_query_number_def: rule-based zero query number suggestion data file.
      suggestion_filter_safe_def_srcs: safe list for suggestion filter.
      usage_dict: usage dictionary data.
      extra_data: a list of any data files to include.
    """
    sources = [
        ":" + name + "@user_pos",
        ":" + name + "@pos_matcher",
        ":" + name + "@collocation",
        ":" + name + "@collocation_suppression",
        ":" + name + "@connection",
        ":" + name + "@dictionary",
        ":" + name + "@suggestion_filter",
        ":" + name + "@pos_group",
        ":" + name + "@boundary",
        ":" + name + "@segmenter",
        ":" + name + "@counter_suffix",
        ":" + name + "@suffix",
        ":" + name + "@reading_correction",
        ":" + name + "@symbol",
        ":" + name + "@emoticon",
        ":" + name + "@emoji",
        ":" + name + "@single_kanji",
        ":" + name + "@single_kanji_noun_prefix",
        ":" + name + "@zero_query",
        ":" + name + "@zero_query_number",
        ":" + name + "@a11y_description",
        ":" + name + "@version",
    ]
    arguments = (
        # TODO(noriyukit): Rename keys to more descriptive names.
        "pos_matcher:32:$(@D)/pos_matcher.data " +
        "user_pos_token:32:$(@D)/user_pos_token_array.data " +
        "user_pos_string:32:$(@D)/user_pos_string_array.data " +
        "coll:32:$(location :" + name + "@collocation) " +
        "cols:32:$(location :" + name + "@collocation_suppression) " +
        "conn:32:$(location :" + name + "@connection) " +
        "dict:32:$(location :" + name + "@dictionary) " +
        "sugg:32:$(location :" + name + "@suggestion_filter) " +
        "posg:32:$(location :" + name + "@pos_group) " +
        "bdry:32:$(location :" + name + "@boundary) " +
        "segmenter_sizeinfo:32:$(@D)/segmenter_sizeinfo.data " +
        "segmenter_ltable:32:$(@D)/segmenter_ltable.data " +
        "segmenter_rtable:32:$(@D)/segmenter_rtable.data " +
        "segmenter_bitarray:32:$(@D)/segmenter_bitarray.data " +
        "counter_suffix:32:$(location :" + name + "@counter_suffix) " +
        "suffix_key:32:$(@D)/suffix_key.data " +
        "suffix_value:32:$(@D)/suffix_value.data " +
        "suffix_token:32:$(@D)/suffix_token.data " +
        "reading_correction_value:32:$(@D)/reading_correction_value.data " +
        "reading_correction_error:32:$(@D)/reading_correction_error.data " +
        "reading_correction_correction:32:$(@D)/reading_correction_correction.data " +
        "symbol_token:32:$(@D)/symbol_token.data " +
        "symbol_string:32:$(@D)/symbol_string.data " +
        "emoticon_token:32:$(@D)/emoticon_token.data " +
        "emoticon_string:32:$(@D)/emoticon_string.data " +
        "emoji_token:32:$(@D)/emoji_token.data " +
        "emoji_string:32:$(@D)/emoji_string.data " +
        "single_kanji_token:32:$(@D)/single_kanji_token.data " +
        "single_kanji_string:32:$(@D)/single_kanji_string.data " +
        "single_kanji_variant_type:32:$(@D)/single_kanji_variant_type.data " +
        "single_kanji_variant_token:32:$(@D)/single_kanji_variant_token.data " +
        "single_kanji_variant_string:32:$(@D)/single_kanji_variant_string.data " +
        "single_kanji_noun_prefix_token:32:$(@D)/single_kanji_noun_prefix_token.data " +
        "single_kanji_noun_prefix_string:32:$(@D)/single_kanji_noun_prefix_string.data " +
        "zero_query_token_array:32:$(@D)/zero_query_token.data " +
        "zero_query_string_array:32:$(@D)/zero_query_string.data " +
        "zero_query_number_token_array:32:$(@D)/zero_query_number_token.data " +
        "zero_query_number_string_array:32:$(@D)/zero_query_number_string.data " +
        "a11y_description_token:32:$(@D)/a11y_description_token.data " +
        "a11y_description_string:32:$(@D)/a11y_description_string.data " +
        "version:32:$(location :" + name + "@version) "
    )
    if usage_dict:
        sources.append(":" + name + "@usage")
        arguments += (
            "usage_base_conjugation_suffix:32:$(@D)/usage_base_conj_suffix.data " +
            "usage_conjugation_suffix:32:$(@D)/usage_conj_suffix.data " +
            "usage_conjugation_index:32:$(@D)/usage_conj_index.data " +
            "usage_item_array:32:$(@D)/usage_item_array.data " +
            "usage_string_array:32:$(@D)/usage_string_array.data "
        )

    for value in extra_data:
        key, alignment, target = value.split(":", 2)
        sources.append(target)
        arguments += "%s:%s:$(location %s) " % (key, alignment, target)

    native.genrule(
        name = name,
        srcs = sources,
        outs = [outs[0]],
        cmd = (
            "$(location //data_manager:dataset_writer_main) " +
            "--magic='" + _byte_array_to_cstring(magic.value) + "' --output=$@ " + arguments
        ),
        tools = ["//data_manager:dataset_writer_main"],
    )

    native.genrule(
        name = name + "@header",
        srcs = [":" + name],
        outs = [outs[1]],
        cmd = (
            "$(location //build_tools:embed_file) " +
            "--input=$< --name=" + varname + " --output=$@"
        ),
        tools = ["//build_tools:embed_file"],
    )

    native.genrule(
        name = name + "@user_pos_manager_data",
        srcs = [
            ":" + name + "@user_pos",
            ":" + name + "@pos_matcher",
        ],
        outs = ["user_pos_manager.data"],
        cmd = (
            "$(location //data_manager:dataset_writer_main) " +
            "--output=$@ " +
            "pos_matcher:32:$(@D)/pos_matcher.data " +
            "user_pos_token:32:$(@D)/user_pos_token_array.data " +
            "user_pos_string:32:$(@D)/user_pos_string_array.data "
        ),
        tools = ["//data_manager:dataset_writer_main"],
    )

    native.genrule(
        name = name + "@user_pos",
        srcs = [
            cforms,
            id_def,
            special_pos,
            user_pos_def,
        ],
        outs = [
            "user_pos_token_array.data",
            "user_pos_string_array.data",
            "pos_list.data",
        ],
        cmd = (
            "$(location //dictionary:gen_user_pos_data) " +
            "--id_file=$(location " + id_def + ") " +
            "--special_pos_file=$(location " + special_pos + ") " +
            "--user_pos_file=$(location " + user_pos_def + ") " +
            "--cforms_file=$(location " + cforms + ") " +
            "--output_token_array=$(location :user_pos_token_array.data) " +
            "--output_string_array=$(location :user_pos_string_array.data) " +
            "--output_pos_list=$(location :pos_list.data)"
        ),
        tools = ["//dictionary:gen_user_pos_data"],
    )

    native.genrule(
        name = name + "@pos_list_header",
        srcs = [":" + name + "@user_pos"],
        outs = [outs[2]],
        cmd = (
            "$(location //build_tools:embed_file) " +
            "--input=$(@D)/pos_list.data --name=kPosArray --output=$@"
        ),
        tools = ["//build_tools:embed_file"],
    )

    native.genrule(
        name = name + "@pos_matcher",
        srcs = [
            id_def,
            special_pos,
            pos_matcher_rule_def,
        ],
        outs = [
            "pos_matcher.data",
        ],
        cmd = (
            "$(location //dictionary:gen_pos_matcher_code) " +
            "--id_file=$(location " + id_def + ") " +
            "--special_pos_file=$(location " + special_pos + ") " +
            "--pos_matcher_rule_file=$(location " + pos_matcher_rule_def + ") " +
            "--output_pos_matcher_data=$@"
        ),
        tools = ["//dictionary:gen_pos_matcher_code"],
    )

    def _get_collocation_error_rate(data_tag):
        return 0.00001 if data_tag != "mock" else 0.000001

    native.genrule(
        name = name + "@collocation",
        srcs = [collocation_src],
        outs = ["collocation.data"],
        cmd = (
            "$(location //rewriter:gen_collocation_data_main) " +
            "--collocation_data=$< --output=$@ --binary_mode --error_rate=%f" % _get_collocation_error_rate(tag)
        ),
        tools = ["//rewriter:gen_collocation_data_main"],
    )

    native.genrule(
        name = name + "@collocation_suppression",
        srcs = [collocation_suppression_src],
        outs = ["collocation_suppression.data"],
        cmd = (
            "$(location //rewriter:gen_collocation_suppression_data_main) " +
            "--suppression_data=$< --output=$@ --binary_mode"
        ),
        tools = ["//rewriter:gen_collocation_suppression_data_main"],
    )

    native.genrule(
        name = name + "@connection",
        srcs = [
            connection_single_column_src,
            id_def,
            special_pos,
        ],
        outs = ["connection.data"],
        cmd = (
            "$(location //data_manager:gen_connection_data) " +
            "--text_connection_file=" +
            "$(location " + connection_single_column_src + ") " +
            "--id_file=$(location " + id_def + ") " +
            "--special_pos_file=$(location " + special_pos + ") " +
            "--binary_output_file=$@ " +
            "--use_1byte_cost=" + use_1byte_cost
        ),
        tools = ["//data_manager:gen_connection_data"],
    )

    native.genrule(
        name = name + "@dictionary",
        srcs = dictionary_srcs + [":" + name + "@user_pos_manager_data"],
        outs = ["dictionary.data"],
        cmd = (
            "$(location //dictionary:gen_system_dictionary_data_main) " +
            "--input=\"" + " ".join(["$(locations %s)" % s for s in dictionary_srcs]) + "\" " +
            "--user_pos_manager_data=$(location :" + name + "@user_pos_manager_data) " +
            "--output=$@"
        ),
        tools = ["//dictionary:gen_system_dictionary_data_main"],
    )

    native.genrule(
        name = name + "@suggestion_filter",
        srcs = [suggestion_filter_src] + suggestion_filter_safe_def_srcs,
        outs = ["suggestion_filter.data"],
        cmd = (
            "$(location //prediction:gen_suggestion_filter_main) " +
            "--input=$(location " + suggestion_filter_src + ") " +
            "--safe_list_files=\"" + ",".join(["$(location %s)" % s for s in suggestion_filter_safe_def_srcs]) + "\" " +
            "--output=$@ " +
            "--header=false "
        ),
        tools = ["//prediction:gen_suggestion_filter_main"],
    )

    native.genrule(
        name = name + "@pos_group",
        srcs = [
            id_def,
            special_pos,
            pos_group_def,
        ],
        outs = ["pos_group.data"],
        cmd = (
            "$(location //dictionary:gen_pos_rewrite_rule) " +
            "--id_def=$(location " + id_def + ") " +
            "--special_pos=$(location " + special_pos + ") " +
            "--pos_group_def=$(location " + pos_group_def + ") " +
            "--output=$@"
        ),
        tools = ["//dictionary:gen_pos_rewrite_rule"],
    )

    native.genrule(
        name = name + "@boundary",
        srcs = [
            boundary_def,
            id_def,
            special_pos,
        ],
        outs = ["boundary.data"],
        cmd = (
            "$(location //converter:gen_boundary_data) " +
            "--boundary_def=$(location " + boundary_def + ") " +
            "--id_def=$(location " + id_def + ") " +
            "--special_pos=$(location " + special_pos + ") " +
            "--output=$@"
        ),
        tools = ["//converter:gen_boundary_data"],
    )

    native.genrule(
        name = name + "@segmenter_inl_header",
        srcs = [
            id_def,
            special_pos,
            segmenter_def,
        ],
        outs = ["segmenter_inl.inc"],
        cmd = ("$(location //converter:gen_segmenter_code) $(SRCS) > $@"),
        tools = ["//converter:gen_segmenter_code"],
    )
    cc_binary(
        name = name + "@segmenter_generator",
        srcs =
            segmenter_generator_srcs +
            [
                ":" + name + "@segmenter_inl_header",
            ],
        visibility = ["//tools:__subpackages__"],
        deps = [
            "//base:init_mozc_buildtool",
            "//converter:gen_segmenter_bitarray",
            "//:macro",
            "@com_google_absl//absl/flags:flag",
            "@com_google_absl//absl/strings",
        ],
    )

    native.genrule(
        name = name + "@segmenter",
        outs = [
            "segmenter_sizeinfo.data",
            "segmenter_ltable.data",
            "segmenter_rtable.data",
            "segmenter_bitarray.data",
        ],
        cmd = (
            "$(location :" + name + "@segmenter_generator) " +
            "--output_size_info=$(@D)/segmenter_sizeinfo.data " +
            "--output_ltable=$(@D)/segmenter_ltable.data " +
            "--output_rtable=$(@D)/segmenter_rtable.data " +
            "--output_bitarray=$(@D)/segmenter_bitarray.data"
        ),
        tools = [":" + name + "@segmenter_generator"],
    )

    native.genrule(
        name = name + "@counter_suffix",
        srcs = dictionary_srcs + [id_def],
        outs = ["counter_suffix.data"],
        cmd = (
            "$(location //rewriter:gen_counter_suffix_array) " +
            "--id_file=$(location " + id_def + ") " +
            "--output=$@ " +
            " ".join(["$(locations %s)" % s for s in dictionary_srcs])
        ),
        tools = ["//rewriter:gen_counter_suffix_array"],
    )

    native.genrule(
        name = name + "@suffix",
        srcs = [suffix],
        outs = [
            "suffix_key.data",
            "suffix_value.data",
            "suffix_token.data",
        ],
        cmd = (
            "$(location //dictionary:gen_suffix_data) " +
            "--input=$< " +
            "--output_key_array=$(@D)/suffix_key.data " +
            "--output_value_array=$(@D)/suffix_value.data " +
            "--output_token_array=$(@D)/suffix_token.data"
        ),
        tools = ["//dictionary:gen_suffix_data"],
    )

    native.genrule(
        name = name + "@reading_correction",
        srcs = [reading_correction_src],
        outs = [
            "reading_correction_value.data",
            "reading_correction_error.data",
            "reading_correction_correction.data",
        ],
        cmd = (
            "$(location //rewriter:gen_reading_correction_data) " +
            "--input=$< " +
            "--output_value_array=$(@D)/reading_correction_value.data " +
            "--output_error_array=$(@D)/reading_correction_error.data " +
            "--output_correction_array=$(@D)/reading_correction_correction.data"
        ),
        tools = ["//rewriter:gen_reading_correction_data"],
    )

    native.genrule(
        name = name + "@symbol",
        srcs = [
            symbol_src,
            symbol_ordering_rule,
            sorting_map,
            ":" + name + "@user_pos_manager_data",
        ],
        outs = [
            "symbol_token.data",
            "symbol_string.data",
        ],
        cmd = (
            "$(location //rewriter:gen_symbol_rewriter_dictionary_main) " +
            "--input=$(location " + symbol_src + ") " +
            "--user_pos_manager_data=$(location :" + name + "@user_pos_manager_data) " +
            "--sorting_table=$(location " + sorting_map + ") " +
            "--ordering_rule=$(location " + symbol_ordering_rule + ") " +
            "--output_token_array=$(location :symbol_token.data) " +
            "--output_string_array=$(location :symbol_string.data)"
        ),
        tools = ["//rewriter:gen_symbol_rewriter_dictionary_main"],
    )

    native.genrule(
        name = name + "@emoticon",
        srcs = [emoticon_src],
        outs = [
            "emoticon_token.data",
            "emoticon_string.data",
        ],
        cmd = (
            "$(location //rewriter:gen_emoticon_rewriter_data) " +
            "--input=$< " +
            "--output_token_array=$(location :emoticon_token.data) " +
            "--output_string_array=$(location :emoticon_string.data)"
        ),
        tools = ["//rewriter:gen_emoticon_rewriter_data"],
    )

    native.genrule(
        name = name + "@emoji",
        srcs = [emoji_src],
        outs = [
            "emoji_token.data",
            "emoji_string.data",
        ],
        cmd = (
            "$(location //rewriter:gen_emoji_rewriter_data) " +
            "--input=$< " +
            "--output_token_array=$(location :emoji_token.data) " +
            "--output_string_array=$(location :emoji_string.data)"
        ),
        tools = ["//rewriter:gen_emoji_rewriter_data"],
    )

    native.genrule(
        name = name + "@single_kanji",
        srcs = [
            single_kanji_src,
            variant_rule,
        ],
        outs = [
            "single_kanji_string.data",
            "single_kanji_token.data",
            "single_kanji_variant_type.data",
            "single_kanji_variant_token.data",
            "single_kanji_variant_string.data",
        ],
        cmd = (
            "$(location //rewriter:gen_single_kanji_rewriter_data) " +
            "--single_kanji_file=$(location " + single_kanji_src + ") " +
            "--variant_file=$(location " + variant_rule + ") " +
            "--output_single_kanji_token=$(location :single_kanji_token.data) " +
            "--output_single_kanji_string=$(location :single_kanji_string.data) " +
            "--output_variant_types=$(location :single_kanji_variant_type.data) " +
            "--output_variant_tokens=$(location :single_kanji_variant_token.data) " +
            "--output_variant_strings=$(location :single_kanji_variant_string.data) "
        ),
        tools = ["//rewriter:gen_single_kanji_rewriter_data"],
    )

    native.genrule(
        name = name + "@single_kanji_noun_prefix",
        outs = [
            "single_kanji_noun_prefix_token.data",
            "single_kanji_noun_prefix_string.data",
        ],
        cmd = (
            "$(location //rewriter:gen_single_kanji_noun_prefix_data) " +
            "--output_token_array=$(location :single_kanji_noun_prefix_token.data) " +
            "--output_string_array=$(location :single_kanji_noun_prefix_string.data)"
        ),
        tools = ["//rewriter:gen_single_kanji_noun_prefix_data"],
    )

    native.genrule(
        name = name + "@zero_query",
        srcs = [
            emoji_src,
            emoticon_categorized_src,
            symbol_src,
            zero_query_def,
        ],
        outs = [
            "zero_query_data.tsv",
            "zero_query_token.data",
            "zero_query_string.data",
        ],
        cmd = (
            "$(location //prediction:gen_zero_query_data) " +
            "--input_rule=$(location " + zero_query_def + ") " +
            "--input_symbol=$(location " + symbol_src + ") " +
            "--input_emoji=$(location " + emoji_src + ") " +
            "--input_emoticon=$(location " + emoticon_categorized_src + ") " +
            "--output_tsv=$(location :zero_query_data.tsv) " +
            "--output_token_array=$(location :zero_query_token.data) " +
            "--output_string_array=$(location :zero_query_string.data)"
        ),
        tools = ["//prediction:gen_zero_query_data"],
    )

    native.genrule(
        name = name + "@zero_query_number",
        srcs = [zero_query_number_def],
        outs = [
            "zero_query_number_token.data",
            "zero_query_number_string.data",
        ],
        cmd = (
            "$(location //prediction:gen_zero_query_number_data) " +
            "--input=$< " +
            "--output_token_array=$(location :zero_query_number_token.data) " +
            "--output_string_array=$(location :zero_query_number_string.data)"
        ),
        tools = ["//prediction:gen_zero_query_number_data"],
    )

    native.genrule(
        name = name + "@a11y_description",
        srcs = [
            a11y_description_src,
        ],
        outs = [
            "a11y_description_token.data",
            "a11y_description_string.data",
        ],
        cmd = (
            "$(location //rewriter:gen_a11y_description_rewriter_data) " +
            "--input=$(location " + a11y_description_src + ") " +
            "--output_token_array=$(location :a11y_description_token.data) " +
            "--output_string_array=$(location :a11y_description_string.data) "
        ),
        tools = ["//rewriter:gen_a11y_description_rewriter_data"],
    )

    native.genrule(
        name = name + "@version",
        srcs = ["//data/version:mozc_version_template.bzl"],
        outs = ["version.data"],
        cmd = (
            "$(location //data_manager:gen_data_version) " +
            "--tag=" + tag + " --mozc_version_template=$< --output=$@"
        ),
        tools = ["//data_manager:gen_data_version"],
    )

    if usage_dict:
        native.genrule(
            name = name + "@usage",
            srcs = [
                cforms,
                usage_dict,
            ],
            outs = [
                "usage_base_conj_suffix.data",
                "usage_conj_index.data",
                "usage_conj_suffix.data",
                "usage_item_array.data",
                "usage_string_array.data",
            ],
            cmd = (
                "$(location //rewriter:gen_usage_rewriter_dictionary_main) " +
                "--usage_data_file=$(location " + usage_dict + ") " +
                "--cforms_file=$(location " + cforms + ") " +
                "--output_base_conjugation_suffix=$(location :usage_base_conj_suffix.data) " +
                "--output_conjugation_suffix=$(location :usage_conj_suffix.data) " +
                "--output_conjugation_index=$(location :usage_conj_index.data) " +
                "--output_usage_item_array=$(location :usage_item_array.data) " +
                "--output_string_array=$(location :usage_string_array.data) "
            ),
            tools = ["//rewriter:gen_usage_rewriter_dictionary_main"],
        )

def _byte_array_to_cstring(byte_array):
    result = ""
    for byte in byte_array:
        if byte < 0 or byte > 255:
            fail("Invalid byte: " + byte)
        high = byte // 16
        low = byte % 16
        result += ("\\x%X%X" % (high, low))
    return result

def magic_number(byte_array):
    return struct(
        value = byte_array,
        length = len(byte_array),
    )
