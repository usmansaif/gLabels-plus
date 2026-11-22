if (Qt6Test_FOUND)

  #=======================================
  # Test SubstitutionField class
  #=======================================
  qt6_wrap_cpp (TestSubstitutionField_moc_sources TestSubstitutionField.hpp)
  add_executable (TestSubstitutionField TestSubstitutionField.cpp ${TestSubstitutionField_moc_sources})
  target_link_libraries (TestSubstitutionField Model Qt6::Test)
  add_test (NAME SubstitutionField COMMAND TestSubstitutionField)

  #=======================================
  # Test data source (merge) mapping, detection and helpers
  #=======================================
  qt6_wrap_cpp (TestDataSource_moc_sources TestDataSource.hpp)
  add_executable (TestDataSource TestDataSource.cpp ${TestDataSource_moc_sources})
  target_link_libraries (TestDataSource Model Qt6::Test)
  add_test (NAME DataSource COMMAND TestDataSource)

  #=======================================
  # Test XmlUtil class
  #=======================================
  qt6_wrap_cpp (TestXmlUtil_moc_sources TestXmlUtil.hpp)
  add_executable (TestXmlUtil TestXmlUtil.cpp ${TestXmlUtil_moc_sources})
  target_link_libraries (TestXmlUtil Model Qt6::Test)
  add_test (NAME XmlUtil COMMAND TestXmlUtil)

  #=======================================
  # Test XmlLabelCreator/Parser classes
  #=======================================
  qt6_wrap_cpp (TestXmlLabel_moc_sources TestXmlLabel.hpp)
  add_executable (TestXmlLabel TestXmlLabel.cpp ${TestXmlLabel_moc_sources})
  target_link_libraries (TestXmlLabel Model Qt6::Test)
  target_compile_definitions(TestXmlLabel PRIVATE TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test (NAME XmlLabel COMMAND TestXmlLabel)

  #=======================================
  # Test object resize handles (grab anywhere along an edge)
  #=======================================
  qt6_wrap_cpp (TestHandles_moc_sources TestHandles.hpp)
  add_executable (TestHandles TestHandles.cpp ${TestHandles_moc_sources})
  target_link_libraries (TestHandles Model Qt6::Test)
  add_test (NAME Handles COMMAND TestHandles)

  #=======================================
  # Test ColorNode class
  #=======================================
  qt6_wrap_cpp (TestColorNode_moc_sources TestColorNode.hpp)
  add_executable (TestColorNode TestColorNode.cpp ${TestColorNode_moc_sources})
  target_link_libraries (TestColorNode Model Qt6::Test)
  add_test (NAME ColorNode COMMAND TestColorNode)

  #=======================================
  # Test FileUtil class
  #=======================================
  qt6_wrap_cpp (TestFileUtil_moc_sources TestFileUtil.hpp)
  add_executable (TestFileUtil TestFileUtil.cpp ${TestFileUtil_moc_sources})
  target_link_libraries (TestFileUtil Model Qt6::Test)
  add_test (NAME FileUtil COMMAND TestFileUtil)

  #=======================================
  # Test Merge classes
  #=======================================
  qt6_wrap_cpp (TestMerge_moc_sources TestMerge.hpp)
  add_executable (TestMerge TestMerge.cpp ${TestMerge_moc_sources})
  target_link_libraries (TestMerge Model Qt6::Test)
  add_test (NAME Merge COMMAND TestMerge)

  #=======================================
  # Test Model class
  #=======================================
  qt6_wrap_cpp (TestModel_moc_sources TestModel.hpp)
  add_executable (TestModel TestModel.cpp ${TestModel_moc_sources})
  target_link_libraries (TestModel Model Qt6::Test)
  add_test (NAME Model COMMAND TestModel)

  #=======================================
  # Test ModelImageObject class
  #=======================================
  qt6_wrap_cpp (TestModelImageObject_moc_sources TestModelImageObject.hpp)
  add_executable (TestModelImageObject TestModelImageObject.cpp ${TestModelImageObject_moc_sources})
  target_link_libraries (TestModelImageObject Model Qt6::Test)
  add_test (NAME ModelImageObject COMMAND TestModelImageObject)

  #=======================================
  # Test RawText class
  #=======================================
  qt6_wrap_cpp (TestRawText_moc_sources TestRawText.hpp)
  add_executable (TestRawText TestRawText.cpp ${TestRawText_moc_sources})
  target_link_libraries (TestRawText Model Qt6::Test)
  add_test (NAME RawText COMMAND TestRawText)

  #=======================================
  # Test TextNode class
  #=======================================
  qt6_wrap_cpp (TestTextNode_moc_sources TestTextNode.hpp)
  add_executable (TestTextNode TestTextNode.cpp ${TestTextNode_moc_sources})
  target_link_libraries (TestTextNode Model Qt6::Test)
  add_test (NAME TextNode COMMAND TestTextNode)

  #=======================================
  # Test Variable class
  #=======================================
  qt6_wrap_cpp (TestVariable_moc_sources TestVariable.hpp)
  add_executable (TestVariable TestVariable.cpp ${TestVariable_moc_sources})
  target_link_libraries (TestVariable Model Qt6::Test)
  add_test (NAME Variable COMMAND TestVariable)

  #=======================================
  # Test Variables class
  #=======================================
  qt6_wrap_cpp (TestVariables_moc_sources TestVariables.hpp)
  add_executable (TestVariables TestVariables.cpp ${TestVariables_moc_sources})
  target_link_libraries (TestVariables Model Qt6::Test)
  add_test (NAME Variables COMMAND TestVariables)

  #=======================================
  # Test StrUtil class
  #=======================================
  qt6_wrap_cpp (TestStrUtil_moc_sources TestStrUtil.hpp)
  add_executable (TestStrUtil TestStrUtil.cpp ${TestStrUtil_moc_sources})
  target_link_libraries (TestStrUtil Model Qt6::Test)
  add_test (NAME StrUtil COMMAND TestStrUtil)

  #=======================================
  # Test SheetLayout (print sheet estimator)
  #=======================================
  qt6_wrap_cpp (TestSheetLayout_moc_sources TestSheetLayout.hpp)
  add_executable (TestSheetLayout TestSheetLayout.cpp ${TestSheetLayout_moc_sources})
  target_link_libraries (TestSheetLayout Model Qt6::Test)
  add_test (NAME SheetLayout COMMAND TestSheetLayout)

  #=======================================
  # Test RollCost (ribbon and label cost estimator)
  #=======================================
  qt6_wrap_cpp (TestRollCost_moc_sources TestRollCost.hpp)
  add_executable (TestRollCost TestRollCost.cpp ${TestRollCost_moc_sources})
  target_link_libraries (TestRollCost Model Qt6::Test)
  add_test (NAME RollCost COMMAND TestRollCost)

endif (Qt6Test_FOUND)
