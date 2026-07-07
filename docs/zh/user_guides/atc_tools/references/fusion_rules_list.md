# 融合规则列表

如下融合规则关闭后可能会有功能使用上的风险；更多融合规则请参见《图融合和UB融合规则参考》。

<!-- npu="950" id1 -->
Ascend 950PR/Ascend 950DT不支持UB融合。
<!-- end id1 -->

<!-- npu="950" id2 -->
- 图融合：Ascend 950PR/Ascend 950DT支持的图融合规则
  - AABiasaddConvFusion
  - AddRmsNormFusionGraphPass
  - ADepthwiseFusionPass
  - Conv2DQuantProcessFusionPass
  - DepthwiseDfFusionPass
  - DepthwiseDwMulFusionPass
  - EinsumPass
  - FixPipeAbilityProcessPass
  - InplaceAddRmsNormFusionPass
  - SoftmaxGradExtFusion
  - TfTagNoConstFoldingFusionPass
  - WeightQuantBatchMatmulV2TransposeFusionPass
  - ZConfusionSoftmaxGradFusionPass
  - ConstToAttrStridedSliceV2Fusion
  - DepthwiseToConv2dFusionPass
  - DWConv2DQuantProcessFusionPass
  - Globalavgpoolpass
<!-- end id2 -->

- 图融合：

    <!-- npu="950" id3 -->
    非Ascend 950PR/Ascend 950DT支持的图融合规则
    <!-- end id3 -->

  - AABiasaddConvFusion
  - AddNFusionPass
  - AddRmsNormFusionGraphPass
  - ADeformableConv2dPass
  - ADepthwiseFusionPass
  - ALSTMFusionPass
  - ApplyAddOutputPass
  - AReduceMeanFusionPass
  - AReduceSumFusionPass
  - AvgPool3DFusionPass
  - AvgPool3DGradFusionPass
  - AvgPoolGradFusionPass
  - AvgPoolQuantProcessFusionPass
  - BatchMatmulV2QuantProcessFusionPass
  - BatchNormBnInferFusionPass
  - BatchNormGradBnInferGradFusion
  - BatchNormGradInfGradFusion
  - BatchMatMulFusionPass
  - CastRemoveFusionPass
  - ConstToAttrPass
  - ConstToAttrReduceSumFusion
  - Conv2DQuantProcessFusionPass
  - CommonLSTMFusionPass
  - CommonSubexpressionEliminationPass：GE公共表达式消除Pass
  - ConstToAttrGatherV2Fusion
  - ConstToAttrResizeNearestNeighborGradFusion
  - ConstToAttrStridedSliceV2Fusion
  - Conv2DTDQuantProcessFusionPass
  - COPYPass
  - DeConvQuantProcessFusionPass
  - DepthwiseDfFusionPass
  - DepthwiseDwMulFusionPass
  - DepthwiseFusionPass
  - DepthwiseToConv2dFusionPass
  - DreluFusionPass
  - DWConv2DQuantProcessFusionPass
  - DynamicGRUV2GradFusionPass
  - DynamicRNNFusionPass
  - DynamicRNNGradAFusionPass
  - DynamicRNNGradAlignFusionPass
  - DynamicRNNGradDAlignFusionPass
  - DynamicRNNGradDFusionPass
  - DynamicRNNGradFusionPass
  - DynamicRNNInsertTransposePass
  - DynamicRNNSeqFusionPass
  - EinsumPass
  - FCQuantProcessFusionPass
  - FixPipeAbilityProcessPass
  - FlattenV2Pass
  - FusedBatchNormBertFusionPass
  - FusedBatchnormFusionPass
  - FusedBatchNormGradFusionPass
  - Globalavgpoolpass
  - GroupConv2DQuantProcessFusionPass
  - HostBNFusionPass
  - InplaceAddRmsNormFusionPass
  - MapIndexFusionPass
  - MatMul2MatMulV2FusionPass
  - MatmulV2QuantProcessFusionPass
  - MaxPoolWithArgmaxFusionPass
  - PackFusionPass
  - PassThroughFusionPass
  - PassThroughSecondFusionPass
  - PermuteFusionPass
  - PoolingQuantProcessFusionPass
  - PReluGradFusionPass
  - PriorBoxPass
  - ProposalFusionPass
  - RNNFusionPass
  - SingleBatchNormFusion
  - SpatialTransformerDPass
  - SPPPass
  - SoftmaxGradExtFusion
  - TbeAntiquantMaxpoolingFusionPass
  - TbeConvDequantS16FusionPass
  - TfMergeConv2DBackpropInputFusionPass
  - TfTagNoConstFoldingFusionPass
  - TransposedUpdateFusionPass
  - TransdataCastFusionPass
  - UnpackFusionPass
  - V100NotRequantFusionPass
  - V200NotRequantFusionPass
  - WeightQuantBatchMatmulV2TransposeFusionPass
  - YoloPass
  - YoloV2DetectionOutputPass
  - YoloV3DetectionOutputV2Pass
  - ZConcatExt2FusionPass
  - ZConcatDFusionPass
  - ZConfusionSoftmaxGradFusionPass
  - ZSplitVDFusionPass
  - ZSplitVFusionPass

- UB融合：
  - ArgMaxWithFusionPass
  - clip\_by\_norm\_nodivsquaresum
  - DeformableOffsetsFusionPass
  - DepthwiseInsertTransDataFusionPass
  - HostShapeOptimizationPass：AI CPU断流水pass
  - NormalizeFusionPass
  - TbeConvRequantFusionPass
  - TbeConvCommonRules0FusionPass
  - TbeConvCommonRules2FusionPass
  - TfMergeSubFusionPass
