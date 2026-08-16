#include "internal/project_transformation_catalogue.h"

#include "internal/project_json.h"
#include "internal/project_runtime.h"

#include <string.h>

typedef struct evo_project_transformation_writer {
    char *bytes;
    size_t capacity;
    size_t size;
    bool failed;
} evo_project_transformation_writer_t;

static const char *const evo_assignment_operator_choices[] = {
    "add",
    "bitwise-and",
    "bitwise-or",
    "bitwise-xor",
    "multiply",
    "subtract"};

static const char *const evo_condition_context_choices[] = {
    "do-while", "for", "if", "while"};

static const evo_project_transformation_parameter_schema_t
    evo_assignment_parameter_schemas[] = {{"operator",
                                           EVO_PROJECT_RECIPE_PARAMETER_CHOICE,
                                           true,
                                           0,
                                           0,
                                           sizeof(evo_assignment_operator_choices) /
                                               sizeof(evo_assignment_operator_choices[0]),
                                           evo_assignment_operator_choices}};

static const evo_project_transformation_parameter_schema_t
    evo_condition_parameter_schemas[] = {{"context",
                                          EVO_PROJECT_RECIPE_PARAMETER_CHOICE,
                                          true,
                                          0,
                                          0,
                                          sizeof(evo_condition_context_choices) /
                                              sizeof(evo_condition_context_choices[0]),
                                          evo_condition_context_choices}};

static const evo_project_transformation_parameter_schema_t
    evo_shift_parameter_schemas[] = {{"maximum-shift",
                                      EVO_PROJECT_RECIPE_PARAMETER_INTEGER,
                                      true,
                                      1,
                                      63,
                                      0U,
                                      NULL}};

static const char *const evo_assignment_preconditions[] = {
    "ast-provider-v1",
    "comments-outside-target",
    "same-nonvolatile-identifier-lvalue",
    "spelling-location-only"};

static const char *const evo_condition_preconditions[] = {
    "ast-provider-v1",
    "scalar-controlling-expression",
    "spelling-location-only",
    "target-free-of-comments"};

static const char *const evo_shift_preconditions[] = {
    "ast-provider-v1",
    "spelling-location-only",
    "target-free-of-comments",
    "unsigned-result-type-preserved"};

static const evo_project_transformation_catalogue_entry_t
    evo_recipe_entries[] = {
        {
            "catalyst.evo.c.assignment-to-compound",
            1U,
            EVO_PROJECT_RECIPE_LOCATION_SPELLING,
            sizeof(evo_assignment_parameter_schemas) /
                sizeof(evo_assignment_parameter_schemas[0]),
            evo_assignment_parameter_schemas,
            sizeof(evo_assignment_preconditions) /
                sizeof(evo_assignment_preconditions[0]),
            evo_assignment_preconditions,
            0U,
            NULL,
            0U,
            NULL,
        },
        {
            "catalyst.evo.c.double-negation-condition",
            1U,
            EVO_PROJECT_RECIPE_LOCATION_SPELLING,
            sizeof(evo_condition_parameter_schemas) /
                sizeof(evo_condition_parameter_schemas[0]),
            evo_condition_parameter_schemas,
            sizeof(evo_condition_preconditions) /
                sizeof(evo_condition_preconditions[0]),
            evo_condition_preconditions,
            0U,
            NULL,
            0U,
            NULL,
        },
        {
            "catalyst.evo.c.unsigned-multiply-to-shift",
            1U,
            EVO_PROJECT_RECIPE_LOCATION_SPELLING,
            sizeof(evo_shift_parameter_schemas) /
                sizeof(evo_shift_parameter_schemas[0]),
            evo_shift_parameter_schemas,
            sizeof(evo_shift_preconditions) /
                sizeof(evo_shift_preconditions[0]),
            evo_shift_preconditions,
            0U,
            NULL,
            0U,
            NULL,
        }};

static const evo_project_transformation_catalogue_t evo_recipe_catalogue = {
    EVO_PROJECT_TRANSFORMATION_CATALOGUE_SCHEMA_VERSION,
    "catalyst.evo.c.ast-transformations",
    1U,
    sizeof(evo_recipe_entries) / sizeof(evo_recipe_entries[0]),
    evo_recipe_entries};

static const evo_project_transformation_ast_form_t evo_assignment_forms[] = {
    EVO_PROJECT_AST_ASSIGNMENT_BINARY,
    EVO_PROJECT_AST_ASSIGNMENT_COMPOUND};
static const evo_project_transformation_ast_form_t evo_condition_forms[] = {
    EVO_PROJECT_AST_DOUBLE_NEGATED_CONDITION,
    EVO_PROJECT_AST_SCALAR_CONDITION};
static const evo_project_transformation_ast_form_t evo_shift_forms[] = {
    EVO_PROJECT_AST_UNSIGNED_MULTIPLY_POWER_OF_TWO,
    EVO_PROJECT_AST_UNSIGNED_SHIFT_POWER_OF_TWO};

static const char *const evo_assignment_assumptions[] = {
    "assignment-and-rhs-reference-the-same-declaration",
    "compound-assignment-preserves-the-provider-resolved-c-type",
    "lvalue-is-a-plain-nonvolatile-identifier",
    "target-is-a-spelling-range-without-comments-or-directives"};

static const char *const evo_condition_assumptions[] = {
    "operand-is-a-c-scalar-controlling-expression",
    "removing-double-negation-preserves-truth-testing",
    "target-is-a-spelling-range-without-comments-or-directives"};

static const char *const evo_shift_assumptions[] = {
    "literal-is-a-decimal-c17-integer-constant",
    "multiplication-result-is-an-unsigned-integer",
    "power-of-two-shift-is-less-than-the-result-width",
    "shift-result-type-matches-the-multiplication-result-type",
    "target-is-a-spelling-range-without-comments-or-directives"};

static const char *const evo_validation_obligations[] = {
    "baseline-build",
    "baseline-correctness",
    "c17-syntax",
    "sanitizers"};

static const evo_project_transformation_capability_t evo_capabilities[] = {
    {
        "catalyst.evo.c.assignment-to-compound",
        1U,
        EVO_PROJECT_TRANSFORMATION_PROVIDER_CONTRACT_VERSION,
        sizeof(evo_assignment_forms) / sizeof(evo_assignment_forms[0]),
        evo_assignment_forms,
        "canonical-c17-spaces-v1",
        "already-satisfied-no-change-v1",
        sizeof(evo_assignment_assumptions) /
            sizeof(evo_assignment_assumptions[0]),
        evo_assignment_assumptions,
        sizeof(evo_validation_obligations) /
            sizeof(evo_validation_obligations[0]),
        evo_validation_obligations,
        false,
        false,
        false,
        false,
    },
    {
        "catalyst.evo.c.double-negation-condition",
        1U,
        EVO_PROJECT_TRANSFORMATION_PROVIDER_CONTRACT_VERSION,
        sizeof(evo_condition_forms) / sizeof(evo_condition_forms[0]),
        evo_condition_forms,
        "preserve-operand-spelling-v1",
        "already-satisfied-no-change-v1",
        sizeof(evo_condition_assumptions) /
            sizeof(evo_condition_assumptions[0]),
        evo_condition_assumptions,
        sizeof(evo_validation_obligations) /
            sizeof(evo_validation_obligations[0]),
        evo_validation_obligations,
        false,
        false,
        false,
        false,
    },
    {
        "catalyst.evo.c.unsigned-multiply-to-shift",
        1U,
        EVO_PROJECT_TRANSFORMATION_PROVIDER_CONTRACT_VERSION,
        sizeof(evo_shift_forms) / sizeof(evo_shift_forms[0]),
        evo_shift_forms,
        "parenthesized-c17-shift-v1",
        "already-satisfied-no-change-v1",
        sizeof(evo_shift_assumptions) / sizeof(evo_shift_assumptions[0]),
        evo_shift_assumptions,
        sizeof(evo_validation_obligations) /
            sizeof(evo_validation_obligations[0]),
        evo_validation_obligations,
        false,
        false,
        false,
        false,
    }};

static void evo_writer_bytes(
    evo_project_transformation_writer_t *writer,
    const char *bytes,
    size_t count)
{
    size_t index;

    if (writer->failed) {
        return;
    }
    if (count > writer->capacity - writer->size) {
        writer->failed = true;
        return;
    }
    if (writer->bytes != NULL) {
        for (index = 0U; index < count; index += 1U) {
            writer->bytes[writer->size + index] = bytes[index];
        }
    }
    writer->size += count;
}

static void evo_writer_text(
    evo_project_transformation_writer_t *writer,
    const char *text)
{
    evo_writer_bytes(writer, text, strlen(text));
}

static void evo_writer_char(
    evo_project_transformation_writer_t *writer,
    char value)
{
    evo_writer_bytes(writer, &value, 1U);
}

static void evo_writer_u64(
    evo_project_transformation_writer_t *writer,
    uint64_t value)
{
    char text[32];
    const int written = evo_project_format(
        text, sizeof(text), "%llu", (unsigned long long)value);

    if (written <= 0 || (size_t)written >= sizeof(text)) {
        writer->failed = true;
        return;
    }
    evo_writer_bytes(writer, text, (size_t)written);
}

static void evo_writer_i64(
    evo_project_transformation_writer_t *writer,
    int64_t value)
{
    char text[32];
    const int written = evo_project_format(
        text, sizeof(text), "%lld", (long long)value);

    if (written <= 0 || (size_t)written >= sizeof(text)) {
        writer->failed = true;
        return;
    }
    evo_writer_bytes(writer, text, (size_t)written);
}

static void evo_writer_json_string(
    evo_project_transformation_writer_t *writer,
    const char *value)
{
    size_t index;

    evo_writer_char(writer, '"');
    for (index = 0U; value[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)value[index];

        if (value[index] == '"') {
            evo_writer_text(writer, "\\\"");
        } else if (value[index] == '\\') {
            evo_writer_text(writer, "\\\\");
        } else if (value[index] == '\n') {
            evo_writer_text(writer, "\\n");
        } else if (value[index] == '\r') {
            evo_writer_text(writer, "\\r");
        } else if (value[index] == '\t') {
            evo_writer_text(writer, "\\t");
        } else if (byte < 0x20U) {
            writer->failed = true;
        } else {
            evo_writer_char(writer, value[index]);
        }
    }
    evo_writer_char(writer, '"');
}

static void evo_writer_text_array(
    evo_project_transformation_writer_t *writer,
    const char *const *values,
    size_t count)
{
    size_t index;

    evo_writer_char(writer, '[');
    for (index = 0U; index < count; index += 1U) {
        if (index > 0U) {
            evo_writer_char(writer, ',');
        }
        evo_writer_json_string(writer, values[index]);
    }
    evo_writer_char(writer, ']');
}

static const char *evo_parameter_kind_name(
    evo_project_recipe_parameter_kind_t kind)
{
    switch (kind) {
    case EVO_PROJECT_RECIPE_PARAMETER_INTEGER:
        return "integer";
    case EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN:
        return "boolean";
    case EVO_PROJECT_RECIPE_PARAMETER_CHOICE:
    default:
        return "choice";
    }
}

static void evo_generate_parameter_schema(
    evo_project_transformation_writer_t *writer,
    const evo_project_transformation_parameter_schema_t *schema)
{
    evo_writer_text(writer, "{\"identity\":");
    evo_writer_json_string(writer, schema->identity);
    evo_writer_text(writer, ",\"kind\":");
    evo_writer_json_string(writer, evo_parameter_kind_name(schema->kind));
    evo_writer_text(writer, ",\"required\":");
    evo_writer_text(writer, schema->required ? "true" : "false");
    evo_writer_text(writer, ",\"minimum_integer\":");
    if (schema->kind == EVO_PROJECT_RECIPE_PARAMETER_INTEGER) {
        evo_writer_i64(writer, schema->minimum_integer);
    } else {
        evo_writer_text(writer, "null");
    }
    evo_writer_text(writer, ",\"maximum_integer\":");
    if (schema->kind == EVO_PROJECT_RECIPE_PARAMETER_INTEGER) {
        evo_writer_i64(writer, schema->maximum_integer);
    } else {
        evo_writer_text(writer, "null");
    }
    evo_writer_text(writer, ",\"choices\":");
    evo_writer_text_array(writer, schema->choices, schema->choice_count);
    evo_writer_char(writer, '}');
}

static void evo_generate_ast_forms(
    evo_project_transformation_writer_t *writer,
    const evo_project_transformation_capability_t *capability)
{
    size_t index;

    evo_writer_char(writer, '[');
    for (index = 0U; index < capability->ast_form_count; index += 1U) {
        if (index > 0U) {
            evo_writer_char(writer, ',');
        }
        evo_writer_json_string(
            writer,
            evo_project_transformation_ast_form_name(
                capability->ast_forms[index]));
    }
    evo_writer_char(writer, ']');
}

static void evo_generate_entry(
    evo_project_transformation_writer_t *writer,
    size_t entry_index)
{
    const evo_project_transformation_catalogue_entry_t *entry =
        &evo_recipe_entries[entry_index];
    const evo_project_transformation_capability_t *capability =
        &evo_capabilities[entry_index];
    size_t index;

    evo_writer_text(writer, "{\"identity\":");
    evo_writer_json_string(writer, entry->identity);
    evo_writer_text(writer, ",\"version\":");
    evo_writer_u64(writer, entry->implementation_version);
    evo_writer_text(writer, ",\"allowed_location_kinds\":[\"spelling\"]");
    evo_writer_text(writer, ",\"parameter_schemas\":[");
    for (index = 0U; index < entry->parameter_schema_count; index += 1U) {
        if (index > 0U) {
            evo_writer_char(writer, ',');
        }
        evo_generate_parameter_schema(writer, &entry->parameter_schemas[index]);
    }
    evo_writer_text(writer, "],\"preconditions\":");
    evo_writer_text_array(
        writer, entry->preconditions, entry->precondition_count);
    evo_writer_text(writer, ",\"dependencies\":[],\"conflicts\":[]");
    evo_writer_text(writer, ",\"capability\":{\"provider_contract_version\":");
    evo_writer_u64(writer, capability->provider_contract_version);
    evo_writer_text(writer, ",\"ast_forms\":");
    evo_generate_ast_forms(writer, capability);
    evo_writer_text(writer, ",\"formatting_policy\":");
    evo_writer_json_string(writer, capability->formatting_policy);
    evo_writer_text(writer, ",\"idempotence_policy\":");
    evo_writer_json_string(writer, capability->idempotence_policy);
    evo_writer_text(writer, ",\"semantic_assumptions\":");
    evo_writer_text_array(
        writer,
        capability->semantic_assumptions,
        capability->semantic_assumption_count);
    evo_writer_text(writer, ",\"validation_obligations\":");
    evo_writer_text_array(
        writer,
        capability->validation_obligations,
        capability->validation_obligation_count);
    evo_writer_text(
        writer,
        ",\"unsupported\":{\"comments\":\"reject\",\"macros\":\"reject\","
        "\"language_extensions\":\"reject\",\"alias_assumptions\":\"reject\"}}}");
}

static void evo_generate_registry_json(
    evo_project_transformation_writer_t *writer)
{
    size_t index;

    evo_writer_text(
        writer,
        "{\n\"schema\":\"catalyst.evo-c-transformation-catalogue.v1\","
        "\n\"schema_version\":1,\n\"catalogue\":{\"identity\":");
    evo_writer_json_string(writer, evo_recipe_catalogue.identity);
    evo_writer_text(writer, ",\"version\":1},\n\"ast_evidence_schema_version\":1,");
    evo_writer_text(writer, "\n\"source_edit_schema_version\":1,\n\"entries\":[");
    for (index = 0U; index < evo_recipe_catalogue.entry_count; index += 1U) {
        if (index > 0U) {
            evo_writer_char(writer, ',');
        }
        evo_writer_char(writer, '\n');
        evo_generate_entry(writer, index);
    }
    evo_writer_text(
        writer,
        "\n],\n\"human_readable_abstraction\":{"
        "\"reference_form\":\"stable-capability-arrays-and-direct-dispatch\","
        "\"projection\":\"complete-catalogue-json-and-derived-markdown\","
        "\"complete\":true,\"probabilistic_authority\":false}\n}\n");
}

static void evo_writer_markdown_value(
    evo_project_transformation_writer_t *writer,
    const char *value)
{
    size_t index;

    for (index = 0U; value[index] != '\0'; index += 1U) {
        if (value[index] == '\\' || value[index] == '|' ||
            value[index] == '`') {
            evo_writer_char(writer, '\\');
        }
        if (value[index] == '\n' || value[index] == '\r') {
            evo_writer_char(writer, ' ');
        } else {
            evo_writer_char(writer, value[index]);
        }
    }
}

static void evo_generate_registry_markdown(
    evo_project_transformation_writer_t *writer)
{
    size_t index;

    evo_writer_text(
        writer,
        "# EVO C Transformation Catalogue\n\n"
        "- Schema: `catalyst.evo-c-transformation-catalogue.v1`\n"
        "- Catalogue: `catalyst.evo.c.ast-transformations` version 1\n"
        "- AST provider contract: version 1\n"
        "- Reference form: stable capability arrays and direct dispatch\n"
        "- Projection complete: yes\n"
        "- Probabilistic authority: no\n\n"
        "| Transformation | Version | AST forms | Formatting | Idempotence |\n"
        "|---|---:|---|---|---|\n");
    for (index = 0U; index <
                     sizeof(evo_capabilities) / sizeof(evo_capabilities[0]);
         index += 1U) {
        const evo_project_transformation_capability_t *capability =
            &evo_capabilities[index];
        size_t form_index;

        evo_writer_text(writer, "| `");
        evo_writer_markdown_value(writer, capability->identity);
        evo_writer_text(writer, "` | 1 | ");
        for (form_index = 0U; form_index < capability->ast_form_count;
             form_index += 1U) {
            if (form_index > 0U) {
                evo_writer_text(writer, ", ");
            }
            evo_writer_markdown_value(
                writer,
                evo_project_transformation_ast_form_name(
                    capability->ast_forms[form_index]));
        }
        evo_writer_text(writer, " | `");
        evo_writer_markdown_value(writer, capability->formatting_policy);
        evo_writer_text(writer, "` | `");
        evo_writer_markdown_value(writer, capability->idempotence_policy);
        evo_writer_text(writer, "` |\n");
    }
    evo_writer_text(
        writer,
        "\nEvery entry rejects comment-crossing edits, macro expansions, language "
        "extensions, ambiguous targets, and unproved alias assumptions. "
        "Complete semantic assumptions and validation obligations are retained "
        "in the canonical JSON. No cache, index, filter, or probabilistic "
        "structure participates in selection or dispatch.\n");
}

static bool evo_static_text_array_valid(
    const char *const *values,
    size_t count,
    size_t maximum_string_bytes)
{
    size_t index;

    for (index = 0U; index < count; index += 1U) {
        if (!evo_project_json_text_valid(
                values[index], maximum_string_bytes, false) ||
            (index > 0U && strcmp(values[index - 1U], values[index]) >= 0)) {
            return false;
        }
    }
    return true;
}

static bool evo_static_catalogue_valid(
    const evo_project_transformation_limits_t *limits)
{
    size_t index;

    if (!evo_project_json_text_valid(
            evo_recipe_catalogue.identity, limits->max_string_bytes, false) ||
        evo_recipe_catalogue.entry_count !=
            sizeof(evo_capabilities) / sizeof(evo_capabilities[0])) {
        return false;
    }
    for (index = 0U; index < evo_recipe_catalogue.entry_count; index += 1U) {
        const evo_project_transformation_catalogue_entry_t *entry =
            &evo_recipe_entries[index];
        const evo_project_transformation_capability_t *capability =
            &evo_capabilities[index];
        size_t schema_index;

        if (entry->identity == NULL || capability->identity == NULL ||
            strcmp(entry->identity, capability->identity) != 0 ||
            entry->implementation_version != capability->implementation_version ||
            entry->allowed_location_kinds !=
                EVO_PROJECT_RECIPE_LOCATION_SPELLING ||
            capability->provider_contract_version !=
                EVO_PROJECT_TRANSFORMATION_PROVIDER_CONTRACT_VERSION ||
            capability->ast_form_count == 0U || capability->ast_forms == NULL ||
            capability->comments_supported || capability->macros_supported ||
            capability->language_extensions_supported ||
            capability->alias_assumptions_supported ||
            !evo_project_json_text_valid(
                entry->identity, limits->max_string_bytes, false) ||
            !evo_project_json_text_valid(
                capability->formatting_policy,
                limits->max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                capability->idempotence_policy,
                limits->max_string_bytes,
                false) ||
            !evo_static_text_array_valid(
                entry->preconditions,
                entry->precondition_count,
                limits->max_string_bytes) ||
            !evo_static_text_array_valid(
                capability->semantic_assumptions,
                capability->semantic_assumption_count,
                limits->max_string_bytes) ||
            !evo_static_text_array_valid(
                capability->validation_obligations,
                capability->validation_obligation_count,
                limits->max_string_bytes) ||
            (index > 0U &&
             strcmp(evo_recipe_entries[index - 1U].identity, entry->identity) >=
                 0)) {
            return false;
        }
        for (schema_index = 0U; schema_index < entry->parameter_schema_count;
             schema_index += 1U) {
            const evo_project_transformation_parameter_schema_t *schema =
                &entry->parameter_schemas[schema_index];

            if (!evo_project_json_text_valid(
                    schema->identity, limits->max_string_bytes, false) ||
                (schema->kind == EVO_PROJECT_RECIPE_PARAMETER_CHOICE &&
                 !evo_static_text_array_valid(
                     schema->choices,
                     schema->choice_count,
                     limits->max_string_bytes))) {
                return false;
            }
        }
    }
    return true;
}

static evo_project_transformation_status_t evo_generate_owned(
    size_t capacity,
    void (*generate)(evo_project_transformation_writer_t *),
    char **bytes,
    size_t *size)
{
    evo_project_transformation_writer_t measure = {NULL, capacity, 0U, false};
    evo_project_transformation_writer_t output;

    generate(&measure);
    if (measure.failed || measure.size == SIZE_MAX) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    *bytes = evo_project_allocate_zeroed(measure.size + 1U, sizeof(**bytes));
    if (*bytes == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    output = (evo_project_transformation_writer_t){
        *bytes, measure.size, 0U, false};
    generate(&output);
    if (output.failed || output.size != measure.size) {
        evo_project_release(*bytes);
        *bytes = NULL;
        return EVO_PROJECT_TRANSFORMATION_ERROR_STATE;
    }
    (*bytes)[output.size] = '\0';
    *size = output.size;
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

const evo_project_transformation_catalogue_t *
evo_project_transformation_builtin_recipe_catalogue(void)
{
    return &evo_recipe_catalogue;
}

const evo_project_transformation_capability_t *
evo_project_transformation_builtin_capabilities(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(evo_capabilities) / sizeof(evo_capabilities[0]);
    }
    return evo_capabilities;
}

const evo_project_transformation_capability_t *
evo_project_transformation_find_capability(
    const char *identity,
    uint32_t implementation_version)
{
    size_t index;

    if (identity == NULL) {
        return NULL;
    }
    for (index = 0U; index <
                     sizeof(evo_capabilities) / sizeof(evo_capabilities[0]);
         index += 1U) {
        if (evo_capabilities[index].implementation_version ==
                implementation_version &&
            strcmp(evo_capabilities[index].identity, identity) == 0) {
            return &evo_capabilities[index];
        }
    }
    return NULL;
}

bool evo_project_transformation_registry_is_builtin(
    const evo_project_transformation_registry_t *registry)
{
    return registry != NULL && registry->private_owner != NULL &&
           registry->schema_version ==
               EVO_PROJECT_TRANSFORMATION_REGISTRY_SCHEMA_VERSION &&
           registry->recipe_catalogue == &evo_recipe_catalogue &&
           registry->capabilities == evo_capabilities &&
           registry->capability_count ==
               sizeof(evo_capabilities) / sizeof(evo_capabilities[0]) &&
           registry->canonical_json != NULL &&
           registry->canonical_json_size > 0U &&
           registry->audit_markdown != NULL &&
           registry->audit_markdown_size > 0U &&
           registry->projection_complete &&
           !registry->probabilistic_authority;
}

evo_project_transformation_status_t
evo_project_transformation_catalogue_generate_evidence(
    const evo_project_transformation_limits_t *limits,
    evo_project_transformation_registry_owner_t *owner)
{
    evo_project_transformation_status_t status;

    if (!evo_static_catalogue_valid(limits)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_CATALOGUE_INVALID;
    }
    status = evo_generate_owned(
        limits->max_registry_bytes,
        evo_generate_registry_json,
        &owner->canonical_json,
        &owner->canonical_json_size);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }
    status = evo_generate_owned(
        limits->max_audit_bytes,
        evo_generate_registry_markdown,
        &owner->audit_markdown,
        &owner->audit_markdown_size);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        evo_project_release(owner->canonical_json);
        owner->canonical_json = NULL;
        owner->canonical_json_size = 0U;
        return status;
    }
    if (owner->canonical_json_size > limits->max_total_bytes ||
        owner->audit_markdown_size >
            limits->max_total_bytes - owner->canonical_json_size) {
        evo_project_release(owner->audit_markdown);
        evo_project_release(owner->canonical_json);
        *owner = (evo_project_transformation_registry_owner_t){0};
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}
