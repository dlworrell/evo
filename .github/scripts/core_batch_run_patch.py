from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f'{label}: expected text not found')
    return text.replace(old, new, 1)


# Thread the private batch evaluator through bounded continuation.
path = Path('src/bounded_run.c')
text = path.read_text()
if 'evo_bounded_run_continue_with_batch_evaluator(' not in text:
    old_signature = '''evo_status_t evo_bounded_run_continue(\n    const evo_problem_t *problem,\n    const evo_config_t *config,\n    void *context,\n    evo_population_t *parents,\n    evo_result_t *best_result,\n    evo_run_state_t *state,\n    evo_bounded_run_evidence_t *evidence)\n'''
    new_signature = '''evo_status_t evo_bounded_run_continue_with_batch_evaluator(\n    const evo_problem_t *problem,\n    const evo_config_t *config,\n    void *context,\n    evo_population_t *parents,\n    evo_result_t *best_result,\n    evo_run_state_t *state,\n    const evo_population_batch_evaluator_t *batch_evaluator,\n    evo_bounded_run_evidence_t *evidence)\n'''
    text = replace_once(text, old_signature, new_signature, 'bounded continue signature')
    old_call = '''        status = evo_child_population_evaluate(problem,\n                                               &transition_config,\n                                               context,\n                                               source_generation,\n                                               &children,\n                                               &evaluation_evidence);\n'''
    new_call = '''        status = batch_evaluator == NULL\n                     ? evo_child_population_evaluate(problem,\n                                                     &transition_config,\n                                                     context,\n                                                     source_generation,\n                                                     &children,\n                                                     &evaluation_evidence)\n                     : evo_child_population_evaluate_with_batch_evaluator(\n                           problem,\n                           &transition_config,\n                           context,\n                           source_generation,\n                           &children,\n                           &evaluation_evidence,\n                           batch_evaluator);\n'''
    text = replace_once(text, old_call, new_call, 'child batch evaluation call')
    wrapper_marker = 'evo_status_t evo_bounded_run_advance(\n'
    wrapper = '''evo_status_t evo_bounded_run_continue(\n    const evo_problem_t *problem,\n    const evo_config_t *config,\n    void *context,\n    evo_population_t *parents,\n    evo_result_t *best_result,\n    evo_run_state_t *state,\n    evo_bounded_run_evidence_t *evidence)\n{\n    return evo_bounded_run_continue_with_batch_evaluator(problem,\n                                                         config,\n                                                         context,\n                                                         parents,\n                                                         best_result,\n                                                         state,\n                                                         NULL,\n                                                         evidence);\n}\n\n'''
    text = replace_once(text, wrapper_marker, wrapper + wrapper_marker, 'bounded advance marker')
path.write_text(text)


# Add private batch-aware top-level run and resume entries while preserving the
# installed public API wrappers.
path = Path('src/population.c')
text = path.read_text()
if '#include "internal/run_batch.h"' not in text:
    text = replace_once(
        text,
        '#include "internal/population_storage.h"\n',
        '#include "internal/population_storage.h"\n#include "internal/population_evaluation.h"\n#include "internal/run_batch.h"\n',
        'population private includes')

if 'evo_run_with_batch_evaluator(' not in text:
    old_signature = 'evo_status_t evo_run(const evo_problem_t *problem, const evo_config_t *config, void *context, evo_result_t *result)\n'
    new_signature = '''evo_status_t evo_run_with_batch_evaluator(\n    const evo_problem_t *problem,\n    const evo_config_t *config,\n    void *context,\n    const evo_population_batch_evaluator_t *batch_evaluator,\n    evo_result_t *result)\n'''
    text = replace_once(text, old_signature, new_signature, 'evo_run signature')
    old_eval = '    status = evo_population_evaluate(problem, config, context, &population);\n'
    new_eval = '''    status = batch_evaluator == NULL\n                 ? evo_population_evaluate(problem, config, context, &population)\n                 : evo_population_evaluate_with_batch_evaluator(problem,\n                                                                config,\n                                                                context,\n                                                                UINT64_C(0),\n                                                                &population,\n                                                                batch_evaluator);\n'''
    text = replace_once(text, old_eval, new_eval, 'initial batch evaluation')
    old_continue = '''        status = evo_bounded_run_continue(problem,\n                                          config,\n                                          context,\n                                          &population,\n                                          result,\n                                          &run_state,\n                                          &run_evidence);\n'''
    new_continue = '''        status = evo_bounded_run_continue_with_batch_evaluator(\n            problem,\n            config,\n            context,\n            &population,\n            result,\n            &run_state,\n            batch_evaluator,\n            &run_evidence);\n'''
    text = replace_once(text, old_continue, new_continue, 'run batch continuation')
    resume_marker = 'evo_status_t evo_resume(const evo_problem_t *problem,\n'
    run_wrapper = '''evo_status_t evo_run(const evo_problem_t *problem,\n                     const evo_config_t *config,\n                     void *context,\n                     evo_result_t *result)\n{\n    return evo_run_with_batch_evaluator(\n        problem, config, context, NULL, result);\n}\n\n'''
    text = replace_once(text, resume_marker, run_wrapper + resume_marker, 'resume marker')

if 'evo_resume_with_batch_evaluator(' not in text:
    old_signature = '''evo_status_t evo_resume(const evo_problem_t *problem,\n                        const evo_config_t *config,\n                        void *context,\n                        const void *checkpoint,\n                        size_t checkpoint_size,\n                        evo_result_t *result)\n'''
    new_signature = '''evo_status_t evo_resume_with_batch_evaluator(\n    const evo_problem_t *problem,\n    const evo_config_t *config,\n    void *context,\n    const void *checkpoint,\n    size_t checkpoint_size,\n    const evo_population_batch_evaluator_t *batch_evaluator,\n    evo_result_t *result)\n'''
    text = replace_once(text, old_signature, new_signature, 'evo_resume signature')
    old_continue = '''        status = evo_bounded_run_continue(problem,\n                                          config,\n                                          context,\n                                          &population,\n                                          result,\n                                          &run_state,\n                                          &run_evidence);\n'''
    new_continue = '''        status = evo_bounded_run_continue_with_batch_evaluator(\n            problem,\n            config,\n            context,\n            &population,\n            result,\n            &run_state,\n            batch_evaluator,\n            &run_evidence);\n'''
    text = replace_once(text, old_continue, new_continue, 'resume batch continuation')
    destroy_marker = 'void evo_result_destroy(evo_result_t *result)\n'
    resume_wrapper = '''evo_status_t evo_resume(const evo_problem_t *problem,\n                        const evo_config_t *config,\n                        void *context,\n                        const void *checkpoint,\n                        size_t checkpoint_size,\n                        evo_result_t *result)\n{\n    return evo_resume_with_batch_evaluator(problem,\n                                           config,\n                                           context,\n                                           checkpoint,\n                                           checkpoint_size,\n                                           NULL,\n                                           result);\n}\n\n'''
    text = replace_once(text, destroy_marker, resume_wrapper + destroy_marker, 'result destroy marker')
path.write_text(text)
