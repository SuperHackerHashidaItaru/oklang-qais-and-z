# baseline diff (advisory)

current run vs the pinned baseline (`bench/baseline.json`). cross-instance
noise is a few percent - read shapes, not decimals. the re-baseline door
is `baseline-diff.py --write` + a human commit.

| benchmark | target | baseline ms | current ms | delta |
|---|---|---:|---:|---:|
| arithmetic_sum | lua | 0.119926794 | 0.127655773 | +6.4% |
| arithmetic_sum | ok-register | 0.19803109200000002 | 0.23788114200000002 | +20.1% |
| arithmetic_sum | python3 | 1.041371745 | 1.011192527 | -2.9% |
| arithmetic_sum | wren | 0.5570499790000001 | 0.5594837220000001 | +0.4% |
| array_for_in_sum | lua | 0.26180704 | 0.264354904 | +1.0% |
| array_for_in_sum | ok-register | 0.13490695700000002 | 0.18200201200000002 | +34.9% |
| array_for_in_sum | python3 | 0.518988756 | 0.52189113 | +0.6% |
| array_for_in_sum | wren | 0.40999629800000004 | 0.412147788 | +0.5% |
| array_for_in_sum_indexed | lua | 0.08245901400000001 | 0.08276842400000001 | +0.4% |
| array_for_in_sum_indexed | ok-register | 0.13456964100000002 | 0.181634122 | +35.0% |
| array_for_in_sum_indexed | python3 | 0.5232509750000001 | 0.522822247 | -0.1% |
| array_for_in_sum_indexed | wren | 0.41127747800000003 | 0.413194435 | +0.5% |
| array_index_sum | lua | 0.111848919 | 0.111582913 | -0.2% |
| array_index_sum | ok-register | 0.289719595 | 0.32575771000000003 | +12.4% |
| array_index_sum | python3 | 0.8458968640000001 | 0.8437647210000001 | -0.3% |
| array_index_sum | wren | 0.49621952500000005 | 0.49794755300000004 | +0.3% |
| array_literal_alloc | lua | 0.021806994 | 0.022302916000000002 | +2.3% |
| array_literal_alloc | ok-register | 0.033740644 | 0.0362708 | +7.5% |
| array_literal_alloc | python3 | 0.059862573 | 0.061387041 | +2.5% |
| array_literal_alloc | wren | 0.060222766000000004 | 0.060761771000000006 | +0.9% |
| array_push_growth | lua | 0.008290824 | 0.00878306 | +5.9% |
| array_push_growth | ok-register | 0.010700492 | 0.011040706 | +3.2% |
| array_push_growth | python3 | 0.034069158 | 0.036182354 | +6.2% |
| array_push_growth | wren | 0.010444724 | 0.010865508000000001 | +4.0% |
| array_subscript_add_compound | lua | 0.030520978 | 0.031836567 | +4.3% |
| array_subscript_add_compound | ok-register | 0.08146765900000001 | 0.09062125500000001 | +11.2% |
| array_subscript_add_compound | python3 | 0.193205703 | 0.19265131800000002 | -0.3% |
| array_subscript_add_plain | lua | 0.031146656 | 0.032530046 | +4.4% |
| array_subscript_add_plain | ok-register | 0.081649133 | 0.090710356 | +11.1% |
| array_subscript_add_plain | python3 | 0.204316784 | 0.19801458100000002 | -3.1% |
| array_subscript_add_plain | wren | 0.125149274 | 0.125781269 | +0.5% |
| array_subscript_variable_add_compound | lua | 0.032932665 | 0.033960046 | +3.1% |
| array_subscript_variable_add_compound | ok-register | 0.084084172 | 0.095169662 | +13.2% |
| array_subscript_variable_add_compound | python3 | 0.19472803000000002 | 0.19656911400000002 | +0.9% |
| array_subscript_variable_add_plain | lua | 0.032948242 | 0.033674121 | +2.2% |
| array_subscript_variable_add_plain | ok-register | 0.089121532 | 0.101360528 | +13.7% |
| array_subscript_variable_add_plain | python3 | 0.224180021 | 0.230819922 | +3.0% |
| array_subscript_variable_add_plain | wren | 0.127484556 | 0.127245453 | -0.2% |
| assign_add_compound | lua | 0.061972492000000004 | 0.062676885 | +1.1% |
| assign_add_compound | ok-register | 0.10263021900000001 | 0.12013811 | +17.1% |
| assign_add_compound | python3 | 0.41246676400000004 | 0.42183034100000005 | +2.3% |
| assign_add_plain | lua | 0.062078703000000006 | 0.062525173 | +0.7% |
| assign_add_plain | ok-register | 0.102359697 | 0.12002937600000001 | +17.3% |
| assign_add_plain | python3 | 0.40547017900000004 | 0.40992766900000005 | +1.1% |
| assign_add_plain | wren | 0.28044072200000003 | 0.280634592 | +0.1% |
| assign_div_compound | lua | 0.023566239000000003 | 0.024169851000000003 | +2.6% |
| assign_div_compound | ok-register | 0.039633624 | 0.046870755 | +18.3% |
| assign_div_compound | python3 | 0.210021271 | 0.207973081 | -1.0% |
| assign_div_plain | lua | 0.023647408 | 0.024219016000000003 | +2.4% |
| assign_div_plain | ok-register | 0.039617664000000004 | 0.046653139 | +17.8% |
| assign_div_plain | python3 | 0.20223453900000002 | 0.202897917 | +0.3% |
| assign_div_plain | wren | 0.11344073 | 0.11364702800000001 | +0.2% |
| assign_mul_compound | lua | 0.026791872 | 0.027258258 | +1.7% |
| assign_mul_compound | ok-register | 0.039165466 | 0.046199849 | +18.0% |
| assign_mul_compound | python3 | 0.188713599 | 0.194980543 | +3.3% |
| assign_mul_plain | lua | 0.026883154000000003 | 0.027175013 | +1.1% |
| assign_mul_plain | ok-register | 0.039120993 | 0.046346354000000006 | +18.5% |
| assign_mul_plain | python3 | 0.190513439 | 0.19126381 | +0.4% |
| assign_mul_plain | wren | 0.11396677000000001 | 0.11450639900000001 | +0.5% |
| assign_sub_compound | lua | 0.063560985 | 0.06401161600000001 | +0.7% |
| assign_sub_compound | ok-register | 0.10098667800000001 | 0.11387019400000001 | +12.8% |
| assign_sub_compound | python3 | 0.450176648 | 0.458944958 | +1.9% |
| assign_sub_plain | lua | 0.063550745 | 0.066215123 | +4.2% |
| assign_sub_plain | ok-register | 0.10094077900000001 | 0.11622433100000001 | +15.1% |
| assign_sub_plain | python3 | 0.448759783 | 0.43422850100000004 | -3.2% |
| assign_sub_plain | wren | 0.281339819 | 0.285910088 | +1.6% |
| branch_toggle | lua | 0.08840007200000001 | 0.08692353700000001 | -1.7% |
| branch_toggle | ok-register | 0.204205939 | 0.212354329 | +4.0% |
| branch_toggle | python3 | 0.522428354 | 0.519373454 | -0.6% |
| branch_toggle | wren | 0.358840384 | 0.362502653 | +1.0% |
| class_dispatch_chain | lua | — | 0.25303048300000003 | — (new target) |
| class_dispatch_chain | ok-register | — | 0.31167331800000003 | — (new target) |
| class_dispatch_chain | python3 | — | 0.576663031 | — (new target) |
| class_dispatch_chain | wren | — | 0.266248111 | — (new target) |
| class_field_loop | lua | — | 0.23065111800000002 | — (new target) |
| class_field_loop | ok-register | — | 0.332606591 | — (new target) |
| class_field_loop | python3 | — | 0.47488542 | — (new target) |
| class_field_loop | wren | — | 0.517936928 | — (new target) |
| class_instantiate | lua | — | 0.44640936600000003 | — (new target) |
| class_instantiate | ok-register | — | 0.568763219 | — (new target) |
| class_instantiate | python3 | — | 0.5551077130000001 | — (new target) |
| class_instantiate | wren | — | 0.317389026 | — (new target) |
| class_method_call | lua | — | 0.190992193 | — (new target) |
| class_method_call | ok-register | — | 0.34174603200000003 | — (new target) |
| class_method_call | python3 | — | 0.584279718 | — (new target) |
| class_method_call | wren | — | 0.27185779600000004 | — (new target) |
| class_operator_add | lua | — | 0.20773907800000002 | — (new target) |
| class_operator_add | ok-register | — | 0.30909883000000005 | — (new target) |
| class_operator_add | python3 | — | 0.272144905 | — (new target) |
| class_operator_add | wren | — | 0.18284439800000002 | — (new target) |
| class_poly_dispatch | lua | — | 0.20754438700000002 | — (new target) |
| class_poly_dispatch | ok-register | — | 0.427476834 | — (new target) |
| class_poly_dispatch | python3 | — | 0.7819485530000001 | — (new target) |
| class_poly_dispatch | wren | — | 0.38259700700000004 | — (new target) |
| closure_upvalue | lua | 0.016909070000000002 | 0.017385249000000002 | +2.8% |
| closure_upvalue | ok-register | 0.031581744 | 0.038424349000000003 | +21.7% |
| closure_upvalue | python3 | 0.068755885 | 0.07037375600000001 | +2.4% |
| closure_upvalue | wren | 0.03183521 | 0.031685954 | -0.5% |
| delta_blue | ok-register | — | 0.17991732800000002 | — (new target) |
| delta_blue | python3 | — | 0.141164915 | — (new target) |
| delta_blue | wren | — | 0.129597506 | — (new target) |
| function_calls | lua | 0.02745312 | 0.027856483 | +1.5% |
| function_calls | ok-register | 0.04970137 | 0.069241842 | +39.3% |
| function_calls | python3 | 0.12272005100000001 | 0.125291927 | +2.1% |
| function_calls | wren | 0.053717657 | 0.054082761 | +0.7% |
| global_access | lua | 0.08821079100000001 | 0.086006049 | -2.5% |
| global_access | ok-register | 0.127653146 | 0.142729504 | +11.8% |
| global_access | python3 | 0.25372035000000004 | 0.256420448 | +1.1% |
| global_access | wren | 0.28107849300000004 | 0.281032534 | -0.0% |
| global_array_subscript_add_compound | lua | 0.044020121 | 0.047164266 | +7.1% |
| global_array_subscript_add_compound | ok-register | 0.08666404500000001 | 0.09625173100000001 | +11.1% |
| global_array_subscript_add_compound | python3 | 0.19157717800000001 | 0.193158765 | +0.8% |
| global_array_subscript_add_plain | lua | 0.044312950000000004 | 0.045059935 | +1.7% |
| global_array_subscript_add_plain | ok-register | 0.09150269300000001 | 0.10171089200000001 | +11.2% |
| global_array_subscript_add_plain | python3 | 0.200820763 | 0.20501052600000003 | +2.1% |
| global_array_subscript_add_plain | wren | 0.125375955 | 0.12490828600000001 | -0.4% |
| local_pressure | lua | 0.046282509 | 0.047375623000000006 | +2.4% |
| local_pressure | ok-register | 0.07624758300000001 | 0.08639426600000001 | +13.3% |
| local_pressure | python3 | 0.322060774 | 0.32793082100000004 | +1.8% |
| local_pressure | wren | 0.15405189 | 0.154426067 | +0.2% |
| loop_counter | lua | 0.094811559 | 0.095286534 | +0.5% |
| loop_counter | ok-register | 0.15090662400000002 | 0.1697419 | +12.5% |
| loop_counter | python3 | 0.49398212900000005 | 0.47487998200000003 | -3.9% |
| loop_counter | wren | 0.279419635 | 0.279289704 | -0.0% |
| map_churn | lua | 0.161128245 | 0.23967232200000002 | +48.7% |
| map_churn | ok-register | 0.196660301 | 0.28427254900000004 | +44.6% |
| map_churn | python3 | 0.090236407 | 0.102605288 | +13.7% |
| map_churn | wren | 0.15988947 | 0.17940704700000001 | +12.2% |
| map_for_in_sum | lua | 0.306913884 | 0.304956634 | -0.6% |
| map_for_in_sum | ok-register | 0.162811949 | 0.199960872 | +22.8% |
| map_for_in_sum | python3 | 0.828296166 | 0.835997455 | +0.9% |
| map_for_in_sum | wren | 0.9338616860000001 | 0.9310651310000001 | -0.3% |
| map_insert | lua | 0.09875499800000001 | 0.194921545 | +97.4% |
| map_insert | ok-register | 0.144721399 | 0.24601355200000002 | +70.0% |
| map_insert | python3 | 0.080645932 | 0.09342404200000001 | +15.8% |
| map_insert | wren | 0.11273300900000001 | 0.137867562 | +22.3% |
| map_lookup | lua | 0.05600485400000001 | 0.057086315000000006 | +1.9% |
| map_lookup | ok-register | 0.24057920800000002 | 0.178660157 | -25.7% |
| map_lookup | python3 | 0.32346717 | 0.328482737 | +1.6% |
| map_lookup | wren | 0.263730114 | 0.263615321 | -0.0% |
| mr_call | lua | 0.042028363000000006 | 0.042576484000000005 | +1.3% |
| mr_call | ok-register | 0.06248565700000001 | 0.088681118 | +41.9% |
| mr_call | python3 | 0.222947903 | 0.223542717 | +0.3% |
| numeric_for_counter | lua | 0.044902314000000006 | 0.045490116000000004 | +1.3% |
| numeric_for_counter | ok-register | 0.10030113700000001 | 0.10849536400000001 | +8.2% |
| numeric_for_counter | python3 | 0.5198600240000001 | 0.534770061 | +2.9% |
| numeric_for_counter | wren | 0.40844389600000003 | 0.408163816 | -0.1% |
| numeric_for_step | lua | 0.024385819000000003 | 0.025011989000000002 | +2.6% |
| numeric_for_step | ok-register | 0.051724122000000004 | 0.055105476 | +6.5% |
| numeric_for_step | python3 | 0.36273668600000003 | 0.36083490300000004 | -0.5% |
| numeric_for_step | wren | 0.278118637 | 0.278761766 | +0.2% |
| numeric_for_sum | lua | 0.04802468 | 0.048495679 | +1.0% |
| numeric_for_sum | ok-register | 0.102623216 | 0.108748993 | +6.0% |
| numeric_for_sum | python3 | 0.6996058380000001 | 0.6872336320000001 | -1.8% |
| numeric_for_sum | wren | 0.396646698 | 0.395920504 | -0.2% |
| recursion_fib | lua | 0.005978837000000001 | 0.006317274 | +5.7% |
| recursion_fib | ok-register | 0.011135276000000001 | 0.016483049 | +48.0% |
| recursion_fib | python3 | 0.021257607 | 0.022456828 | +5.6% |
| recursion_fib | wren | 0.011968254000000001 | 0.012362758000000001 | +3.3% |
| varargs_pack | lua | 0.05481089 | 0.05562186 | +1.5% |
| varargs_pack | ok-register | 0.043033613000000005 | 0.051032599000000005 | +18.6% |
| varargs_pack | python3 | 0.06548361100000001 | 0.065826359 | +0.5% |
