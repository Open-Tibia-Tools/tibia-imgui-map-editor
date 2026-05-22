/**
 * @file BrushXmlReader.cpp
 * @brief Implementation of RME-format brush XML parsing.
 */

#include "BrushXmlReader.h"

#include "../Brushes/BrushRegistry.h"
#include "../Brushes/Behaviors/WeightedSelection.h"
#include "../Brushes/Types/CarpetBrush.h"
#include "../Brushes/Types/DoodadBrush.h"
#include "../Brushes/Types/GroundBrush.h"
#include "../Brushes/Types/WallDecorationBrush.h"
#include "../Brushes/Types/TableBrush.h"
#include "../Brushes/Types/WallBrush.h"
#include "../Services/Brushes/BorderLookupService.h"
#include "../Services/Brushes/CarpetLookupService.h"
#include "../Services/Brushes/TableLookupService.h"
#include "../Services/Brushes/WallLookupService.h"
#include "XmlUtils.h"
#include <algorithm>
#include <charconv>
#include <optional>
#include <spdlog/spdlog.h>
#include <system_error>
#include <string_view>

namespace MapEditor::IO {

namespace fs = std::filesystem;
using namespace Brushes;

namespace {

uint32_t parseChance(const pugi::xml_node &node) {
  return node.attribute("chance").as_uint(1);
}

uint16_t parseItemId(const pugi::xml_node &node) {
  return static_cast<uint16_t>(
      node.attribute("id").as_uint(node.attribute("item").as_uint(0)));
}

std::optional<int32_t> parseInt(std::string_view text) {
  int32_t value = 0;
  const auto *begin = text.data();
  const auto *end = begin + text.size();
  const auto [ptr, ec] = std::from_chars(begin, end, value);
  if (ec != std::errc{} || ptr != end) {
    return std::nullopt;
  }
  return value;
}

std::optional<float> parseThicknessValue(std::string_view thickness) {
  if (thickness.empty()) {
    return std::nullopt;
  }

  if (const auto slash = thickness.find('/'); slash != std::string_view::npos) {
    const auto numerator = parseInt(thickness.substr(0, slash));
    const auto denominator = parseInt(thickness.substr(slash + 1));
    if (!numerator || !denominator) {
      return std::nullopt;
    }

    const auto safeNumerator = std::max(0, *numerator);
    const auto safeDenominator = std::max(1, *denominator);
    return static_cast<float>(safeNumerator) /
           static_cast<float>(safeDenominator);
  }

  const auto value = parseInt(thickness);
  if (!value) {
    return std::nullopt;
  }

  if (*value <= 1) {
    return static_cast<float>(*value);
  }

  return static_cast<float>(*value) / 100.0f;
}

bool readBoolAttribute(const pugi::xml_node &node, const char *name,
                       bool defaultValue = false) {
  const auto attribute = node.attribute(name);
  return attribute ? attribute.as_bool(defaultValue) : defaultValue;
}

bool readBoolAttributeAlias(const pugi::xml_node &node, const char *primary,
                            const char *alias, bool defaultValue = false) {
  const auto primaryAttribute = node.attribute(primary);
  if (primaryAttribute) {
    return primaryAttribute.as_bool(defaultValue);
  }

  const auto aliasAttribute = node.attribute(alias);
  if (aliasAttribute) {
    return aliasAttribute.as_bool(defaultValue);
  }

  return defaultValue;
}

BrushPreviewDescriptor parsePreviewDescriptor(const pugi::xml_node &node) {
  if (const auto serverLookId = node.attribute("server_lookid").as_uint(0);
      serverLookId != 0) {
    return BrushPreviewDescriptor::serverItem(serverLookId);
  }

  if (const auto lookId = node.attribute("lookid").as_uint(0); lookId != 0) {
    return BrushPreviewDescriptor::serverItem(lookId);
  }

  return {};
}

bool isCollectionSourceFile(const fs::path &sourceFile) {
  std::string filename = sourceFile.filename().string();
  std::transform(filename.begin(), filename.end(), filename.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return filename.find("collection") != std::string::npos;
}

void markBrushMetadata(IBrush &brush, const fs::path &sourceFile) {
  brush.flagAsVisible();
  if (isCollectionSourceFile(sourceFile)) {
    brush.setCollection();
  }
}

GroundBrush::BorderRule parseInlineBorderRule(const pugi::xml_node &borderNode,
                                              BrushRegistry &registry) {
  GroundBrush::BorderRule rule;
  rule.outer = std::string_view(borderNode.attribute("align").as_string()) ==
               "outer";
  rule.superBorder = borderNode.attribute("super").as_bool(false);
  rule.targetName = borderNode.attribute("to").as_string();
  rule.targetNone = rule.targetName == "none";
  rule.block.setGroundEquivalent(borderNode.attribute("ground_equivalent").as_uint(0));

  for (const auto borderItem : borderNode.children("borderitem")) {
    const auto edge = parseEdgeName(borderItem.attribute("edge").as_string());
    const auto itemId = parseItemId(borderItem);
    if (edge != EdgeType::None && itemId != 0) {
      rule.block.addItem(edge, itemId, parseChance(borderItem));
    }
  }

  for (const auto specificNode : borderNode.children("specific")) {
    SpecificCaseBlock specificCase;
    specificCase.setKeepBorder(specificNode.attribute("keep_border").as_bool(false));

    for (const auto conditionsNode : specificNode.children("conditions")) {
      for (const auto matchNode : conditionsNode.children()) {
        const auto nodeName = std::string_view(matchNode.name());
        if (nodeName == "match_border") {
          const auto borderId = matchNode.attribute("id").as_uint(0);
          const auto edge = parseEdgeName(matchNode.attribute("edge").as_string());
          if (const auto *templateBlock = registry.getBorderTemplate(borderId);
              templateBlock && edge != EdgeType::None) {
            specificCase.addMatchItem(templateBlock->getPrimaryItem(edge));
          }
        } else if (nodeName == "match_group") {
          const auto matchGroup = matchNode.attribute("group").as_uint(0);
          const auto edge = parseEdgeName(matchNode.attribute("edge").as_string());
          if (matchGroup != 0 && edge != EdgeType::None) {
            specificCase.setMatchGroup(matchGroup, edge);
          }
        } else if (nodeName == "match_item") {
          specificCase.addMatchItem(matchNode.attribute("id").as_uint(0));
        }
      }
    }

    for (const auto actionsNode : specificNode.children("actions")) {
      for (const auto actionNode : actionsNode.children()) {
        const auto nodeName = std::string_view(actionNode.name());
        if (nodeName == "replace_border") {
          const auto borderId = actionNode.attribute("id").as_uint(0);
          const auto edge = parseEdgeName(actionNode.attribute("edge").as_string());
          const auto withId = actionNode.attribute("with").as_uint(0);
          if (const auto *templateBlock = registry.getBorderTemplate(borderId);
              templateBlock && edge != EdgeType::None && withId != 0) {
            specificCase.setReplaceAction(templateBlock->getPrimaryItem(edge), withId);
          }
        } else if (nodeName == "replace_item") {
          const auto toReplaceId = actionNode.attribute("id").as_uint(0);
          const auto withId = actionNode.attribute("with").as_uint(0);
          if (toReplaceId != 0 && withId != 0) {
            specificCase.setReplaceAction(toReplaceId, withId);
          }
        } else if (nodeName == "delete_borders") {
          specificCase.setDeleteAll(true);
        }
      }
    }

    if (specificCase.getRequiredMatchCount() > 0) {
      rule.block.addSpecificCase(std::move(specificCase));
    }
  }

  return rule;
}

CompositeItem parseCompositeNode(const pugi::xml_node &compositeNode) {
  CompositeItem composite;
  composite.chance = parseChance(compositeNode);

  for (const auto tileNode : compositeNode.children("tile")) {
    CompositeItem::TileOffset offset;
    offset.dx = tileNode.attribute("x").as_int(0);
    offset.dy = tileNode.attribute("y").as_int(0);
    offset.dz = tileNode.attribute("z").as_int(0);

    for (const auto itemNode : tileNode.children("item")) {
      const auto itemId = parseItemId(itemNode);
      if (itemId == 0) {
        continue;
      }
      offset.items.push_back(
          {.itemId = itemId,
           .chance = parseChance(itemNode),
           .subtype = itemNode.attribute("subtype").as_uint(0)});
    }

    if (!offset.items.empty()) {
      composite.tiles.push_back(std::move(offset));
    }
  }

  return composite;
}

template <typename TBrush>
void parseWallLikeBrush(const pugi::xml_node &node, const std::string &name,
                        uint32_t lookId, const fs::path &sourceFile,
                        BrushRegistry &registry) {
  auto brush = std::make_unique<TBrush>(name, lookId, registry);
  brush->setPreviewDescriptor(parsePreviewDescriptor(node));
  markBrushMetadata(*brush, sourceFile);

  for (const auto wallNode : node.children("wall")) {
    const auto align = parseWallType(wallNode.attribute("type").as_string());

    for (const auto itemNode : wallNode.children("item")) {
      const auto itemId = parseItemId(itemNode);
      if (itemId != 0) {
        brush->addWallItem(align, itemId, parseChance(itemNode));
      }
    }

    for (const auto doorNode : wallNode.children("door")) {
      const auto itemId = parseItemId(doorNode);
      if (itemId == 0) {
        continue;
      }

      DoorNode entry;
      entry.type = parseDoorType(doorNode.attribute("type").as_string());
      entry.alignment = align;
      entry.items.push_back(itemId);
      entry.isOpen = doorNode.attribute("open").as_bool(false);
      entry.isLocked = doorNode.attribute("locked").as_bool(false);
      brush->addDoorItem(align, std::move(entry));
    }
  }

  for (const auto friendNode : node.children("friend")) {
    if (friendNode.attribute("redirect").as_bool(false)) {
      brush->addRedirectName(friendNode.attribute("name").as_string());
    }
  }

  registry.addBrush(std::move(brush));
}

} // namespace

BrushXmlReader::BrushXmlReader(Dependencies deps) : deps_(std::move(deps)) {}

bool BrushXmlReader::loadFile(const fs::path &path) {
  if (!fs::exists(path)) {
    spdlog::warn("[BrushXmlReader] File not found: {}", path.string());
    return false;
  }

  std::string absPath = fs::absolute(path).string();
  if (loadedFiles_.contains(absPath)) {
    return true;
  }
  loadedFiles_.insert(absPath);

  pugi::xml_document doc;
  std::string error;
  pugi::xml_node root = XmlUtils::loadXmlFile(path, "brushes", doc, error);
  if (!root) {
    root = XmlUtils::loadXmlFile(path, "materials", doc, error);
    if (!root) {
      spdlog::error("[BrushXmlReader] {}", error);
      return false;
    }
  }

  lastLoadCount_ = 0;
  parseBrushesRoot(root, path);
  spdlog::info("[BrushXmlReader] Loaded {} brushes from {}",
               lastLoadCount_, path.filename().string());
  return true;
}

size_t BrushXmlReader::loadDirectory(const fs::path &dir) {
  if (!fs::is_directory(dir)) {
    return 0;
  }

  size_t totalLoaded = 0;
  for (const auto &entry : fs::directory_iterator(dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".xml" &&
        loadFile(entry.path())) {
      totalLoaded += lastLoadCount_;
    }
  }
  return totalLoaded;
}

void BrushXmlReader::parseBrushesRoot(const pugi::xml_node &root,
                                      const fs::path &sourceFile) {
  for (const auto child : root.children()) {
    const std::string_view nodeName = child.name();
    if (nodeName == "brush" || nodeName == "ground" || nodeName == "wall" ||
        nodeName == "doodad" || nodeName == "table" || nodeName == "carpet") {
      parseBrush(child, sourceFile);
    }
  }
}

void BrushXmlReader::parseBrush(const pugi::xml_node &node,
                                const fs::path &sourceFile) {
  const std::string name = node.attribute("name").as_string();
  if (name.empty() || !deps_.brushRegistry) {
    return;
  }

  std::string typeStr = node.attribute("type").as_string();
  if (typeStr.empty()) {
    typeStr = node.name();
  }

  auto lookId = node.attribute("server_lookid").as_uint(
      node.attribute("lookid").as_uint(0));

  if (typeStr == "ground" || typeStr == "border") {
    parseGroundBrush(node, name, lookId, sourceFile);
  } else if (typeStr == "wall") {
    parseWallBrush(node, name, lookId, sourceFile);
  } else if (typeStr == "wall decoration") {
    parseWallDecorationBrush(node, name, lookId, sourceFile);
  } else if (typeStr == "doodad") {
    parseDoodadBrush(node, name, lookId, sourceFile);
  } else if (typeStr == "table") {
    parseTableBrush(node, name, lookId, sourceFile);
  } else if (typeStr == "carpet") {
    parseCarpetBrush(node, name, lookId, sourceFile);
  } else {
    spdlog::debug("[BrushXmlReader] Unsupported brush type '{}' for '{}'",
                  typeStr, name);
    return;
  }

  ++lastLoadCount_;
}

void BrushXmlReader::parseGroundBrush(const pugi::xml_node &node,
                                      const std::string &name,
                                      uint32_t lookId,
                                      const fs::path &sourceFile) {
  auto brush =
      std::make_unique<GroundBrush>(name, lookId, *deps_.brushRegistry);
  brush->setPreviewDescriptor(parsePreviewDescriptor(node));
  markBrushMetadata(*brush, sourceFile);
  brush->setZOrder(node.attribute("z-order").as_int(0));

  for (const auto itemNode : node.children("item")) {
    const auto itemId = parseItemId(itemNode);
    if (itemId != 0) {
      brush->addGroundItem(itemId, parseChance(itemNode));
    }
  }

  for (const auto borderNode : node.children("border")) {
    GroundBrush::BorderRule rule =
        parseInlineBorderRule(borderNode, *deps_.brushRegistry);
    if (!borderNode.child("borderitem")) {
      const auto borderId = borderNode.attribute("id").as_uint(0);
      if (const auto *templateBlock =
              deps_.brushRegistry->getBorderTemplate(borderId)) {
        auto mergedBlock = *templateBlock;
        mergedBlock.setGroundEquivalent(
            borderNode.attribute("ground_equivalent")
                .as_uint(templateBlock->getGroundEquivalent()));
        for (const auto &specificCase : rule.block.getSpecificCases()) {
          mergedBlock.addSpecificCase(specificCase);
        }
        rule.block = std::move(mergedBlock);
      }
    }
    brush->addBorderRule(std::move(rule));
  }

  if (const auto optionalNode = node.child("optional")) {
    const auto borderId = optionalNode.attribute("id").as_uint(0);
    if (const auto *templateBlock =
            deps_.brushRegistry->getBorderTemplate(borderId)) {
      brush->setOptionalBorder(*templateBlock,
                               node.attribute("solo_optional").as_bool(false));
    }
  }

  for (const auto friendNode : node.children("friend")) {
    brush->addFriend(friendNode.attribute("name").as_string());
  }
  for (const auto enemyNode : node.children("enemy")) {
    brush->addEnemy(enemyNode.attribute("name").as_string());
  }

  deps_.brushRegistry->addBrush(std::move(brush));
}

void BrushXmlReader::parseWallBrush(const pugi::xml_node &node,
                                    const std::string &name, uint32_t lookId,
                                    const fs::path &sourceFile) {
  parseWallLikeBrush<WallBrush>(node, name, lookId, sourceFile,
                                *deps_.brushRegistry);
}

void BrushXmlReader::parseWallDecorationBrush(const pugi::xml_node &node,
                                              const std::string &name,
                                              uint32_t lookId,
                                              const fs::path &sourceFile) {
  parseWallLikeBrush<WallDecorationBrush>(node, name, lookId, sourceFile,
                                          *deps_.brushRegistry);
}

void BrushXmlReader::parseDoodadBrush(const pugi::xml_node &node,
                                      const std::string &name,
                                      uint32_t lookId,
                                      const fs::path &sourceFile) {
  auto brush = std::make_unique<DoodadBrush>(
      name, lookId, *deps_.brushRegistry,
      readBoolAttribute(node, "draggable", true));
  brush->setPreviewDescriptor(parsePreviewDescriptor(node));
  markBrushMetadata(*brush, sourceFile);
  brush->setRedoBorders(
      readBoolAttributeAlias(node, "redo_borders", "reborder", false));
  brush->setOneSize(readBoolAttribute(node, "one_size", false));
  brush->setRemoveOptionalBorder(
      readBoolAttribute(node, "remove_optional_border", false));
  brush->setOnBlocking(readBoolAttribute(node, "on_blocking", false));
  brush->setOnDuplicate(readBoolAttribute(node, "on_duplicate", false));

  if (brush->removesOptionalBorder() && !brush->needsBorderUpdate()) {
    spdlog::warn(
        "[BrushXmlReader] remove_optional_border on '{}' has no effect without redo_borders/reborder",
        name);
  }

  if (const auto thickness = parseThicknessValue(node.attribute("thickness").as_string());
      thickness.has_value()) {
    brush->setThickness(*thickness);
  }

  auto appendSingles = [&](const pugi::xml_node &parent,
                           DoodadAlternative &alternative) {
    for (const auto itemNode : parent.children("item")) {
      const auto itemId = parseItemId(itemNode);
      if (itemId != 0) {
        alternative.addSingleItem(
            {.itemId = itemId,
             .chance = parseChance(itemNode),
             .subtype = itemNode.attribute("subtype").as_uint(0)});
      }
    }
  };

  bool addedAlternative = false;
  for (const auto altNode : node.children("alternate")) {
    DoodadAlternative alternative;
    appendSingles(altNode, alternative);
    for (const auto compositeNode : altNode.children("composite")) {
      auto composite = parseCompositeNode(compositeNode);
      if (!composite.tiles.empty()) {
        alternative.addComposite(std::move(composite));
      }
    }
    if (alternative.hasContent()) {
      brush->addAlternative(std::move(alternative));
      addedAlternative = true;
    }
  }

  if (!addedAlternative) {
    DoodadAlternative alternative;
    appendSingles(node, alternative);
    for (const auto compositeNode : node.children("composite")) {
      auto composite = parseCompositeNode(compositeNode);
      if (!composite.tiles.empty()) {
        alternative.addComposite(std::move(composite));
      }
    }
    if (alternative.hasContent()) {
      brush->addAlternative(std::move(alternative));
    }
  }

  deps_.brushRegistry->addBrush(std::move(brush));
}

void BrushXmlReader::parseTableBrush(const pugi::xml_node &node,
                                     const std::string &name, uint32_t lookId,
                                     const fs::path &sourceFile) {
  auto brush =
      std::make_unique<TableBrush>(name, lookId, *deps_.brushRegistry);
  brush->setPreviewDescriptor(parsePreviewDescriptor(node));
  markBrushMetadata(*brush, sourceFile);

  for (const auto tableNode : node.children("table")) {
    const auto align = parseTableAlign(tableNode.attribute("align").as_string());

    if (const auto itemId = parseItemId(tableNode); itemId != 0) {
      brush->addAlignedItem(align, itemId, parseChance(tableNode));
    }

    for (const auto itemNode : tableNode.children("item")) {
      const auto itemId = parseItemId(itemNode);
      if (itemId != 0) {
        brush->addAlignedItem(align, itemId, parseChance(itemNode));
      }
    }
  }

  deps_.brushRegistry->addBrush(std::move(brush));
}

void BrushXmlReader::parseCarpetBrush(const pugi::xml_node &node,
                                      const std::string &name,
                                      uint32_t lookId,
                                      const fs::path &sourceFile) {
  auto brush =
      std::make_unique<CarpetBrush>(name, lookId, *deps_.brushRegistry);
  brush->setPreviewDescriptor(parsePreviewDescriptor(node));
  markBrushMetadata(*brush, sourceFile);

  for (const auto carpetNode : node.children("carpet")) {
    const auto align = parseEdgeName(carpetNode.attribute("align").as_string());

    if (const auto itemId = parseItemId(carpetNode); itemId != 0) {
      brush->addAlignedItem(align, itemId, parseChance(carpetNode));
    }

    for (const auto itemNode : carpetNode.children("item")) {
      const auto itemId = parseItemId(itemNode);
      if (itemId != 0) {
        brush->addAlignedItem(align, itemId, parseChance(itemNode));
      }
    }
  }

  deps_.brushRegistry->addBrush(std::move(brush));
}

} // namespace MapEditor::IO
