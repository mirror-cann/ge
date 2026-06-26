# Naming Conventions

## 1. File Naming

Split files and name them at the feature granularity level

    OK: test_ctrl_flow_infershape.cc

        This indicates that this group of test cases primarily tests control flow infershape-related features.

    **NOK: test_graph_compiler.cc**

        It's unclear what feature is being tested.

## 2. Test Case Naming

Test case names need to clearly indicate the testing point

    OK:    TEST_F(ConstantFoldingTest, test_flattenv2_folding_success)

        This indicates that this test case tests the normal scenario of flattenv2's constant folding functionality

    **NOK:  TEST_F(FftsPlusTest, ffts_plus_graph)**

        It's unclear what functionality is being tested.

# Verification Points

## 1. Test cases must have verification points; empty flow test cases are not allowed

`// build_graph through session`

 `ret = session.BuildGraph(2, inputs);`

 `EXPECT_EQ(ret, SUCCESS);`

Only verified the interface return success, with no other verification points. This basically has no guarding function.

## 2. Test cases need to be end-to-end entry test cases; copying UT test cases is not allowed
