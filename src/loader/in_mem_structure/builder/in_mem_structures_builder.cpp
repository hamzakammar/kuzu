#include "src/loader/include/in_mem_structure/builder/in_mem_structures_builder.h"

namespace graphflow {
namespace loader {

InMemStructuresBuilder::InMemStructuresBuilder(
    TaskScheduler& taskScheduler, const Graph& graph, string outputDirectory)
    : logger{LoggerUtils::getOrCreateSpdLogger("loader")},
      taskScheduler{taskScheduler}, graph{graph}, outputDirectory{move(outputDirectory)} {}

void InMemStructuresBuilderForRels::populateNumRelsInfo(
    vector<vector<vector<uint64_t>>>& numRelsPerDirBoundLabelRelLabel, bool forColumns) {
    for (auto& direction : DIRECTIONS) {
        if (forColumns) {
            if (description.isSingleMultiplicityPerDirection[direction]) {
                for (auto boundNodeLabel : description.nodeLabelsPerDirection[direction]) {
                    numRelsPerDirBoundLabelRelLabel[direction][boundNodeLabel][description.label] =
                        (*directionLabelNumRels[direction])[boundNodeLabel].load();
                }
            }
        } else {
            if (!description.isSingleMultiplicityPerDirection[direction]) {
                for (auto boundNodeLabel : description.nodeLabelsPerDirection[direction]) {
                    numRelsPerDirBoundLabelRelLabel[direction][boundNodeLabel][description.label] =
                        (*directionLabelNumRels[direction])[boundNodeLabel].load();
                }
            }
        }
    }
}

} // namespace loader
} // namespace graphflow
