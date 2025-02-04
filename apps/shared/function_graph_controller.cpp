#include "function_graph_controller.h"

#include <apps/apps_container_helper.h>
#include <assert.h>
#include <float.h>
#include <poincare/coordinate_2D.h>
#include <poincare/helpers.h>
#include <poincare/layout_helper.h>

#include <algorithm>
#include <cmath>

#include "function_app.h"
#include "poincare_helpers.h"

using namespace Escher;
using namespace Poincare;

namespace Shared {

FunctionGraphController::FunctionGraphController(
    Responder *parentResponder,
    Escher::InputEventHandlerDelegate *inputEventHandlerDelegate,
    ButtonRowController *header, InteractiveCurveViewRange *interactiveRange,
    AbstractPlotView *curveView, CurveViewCursor *cursor,
    int *selectedCurveIndex)
    : InteractiveCurveViewController(
          parentResponder, inputEventHandlerDelegate, header, interactiveRange,
          curveView, cursor, I18n::Message::GraphCalculus, selectedCurveIndex) {
}

void FunctionGraphController::didBecomeFirstResponder() {
  if (curveView()->hasFocus()) {
    bannerView()->abscissaValue()->setParentResponder(this);
    bannerView()->abscissaValue()->setDelegates(textFieldDelegateApp(), this);
    if (!isAlongY(*m_selectedCurveIndex)) {
      Container::activeApp()->setFirstResponder(bannerView()->abscissaValue());
    }
  } else {
    InteractiveCurveViewController::didBecomeFirstResponder();
  }
}

void FunctionGraphController::viewWillAppear() {
  functionGraphView()->setBannerView(bannerView());
  functionGraphView()->setAreaHighlight(NAN, NAN);

  if (functionGraphView()->context() == nullptr) {
    functionGraphView()->setContext(textFieldDelegateApp()->localContext());
  }

  InteractiveCurveViewController::viewWillAppear();
  selectCurveAtIndex(*m_selectedCurveIndex, true);
}

void FunctionGraphController::openMenuForCurveAtIndex(int curveIndex) {
  if (curveIndex != *m_selectedCurveIndex) {
    selectCurveAtIndex(curveIndex, false);
    Coordinate2D<double> xy = xyValues(curveIndex, m_cursor->t(),
                                       textFieldDelegateApp()->localContext(),
                                       m_selectedSubCurveIndex);
    m_cursor->moveTo(m_cursor->t(), xy.x(), xy.y());
  }
  curveParameterControllerWithRecord()->setRecord(recordAtSelectedCurveIndex());
  stackController()->push(curveParameterController());
}

void FunctionGraphController::selectCurveAtIndex(int curveIndex,
                                                 bool willBeVisible) {
  if (curveIndex != *m_selectedCurveIndex) {
    m_selectedSubCurveIndex = 0;
    *m_selectedCurveIndex = curveIndex;
  }

  Ion::Storage::Record r = recordAtCurveIndex(curveIndex);
  functionGraphView()->selectRecord(r);
  functionGraphView()->cursorView()->setColor(
      functionStore()->colorForRecord(r), functionGraphView());
  // Force reload to display the selected function on top
  if (willBeVisible) {
    functionGraphView()->reload(false, true);
  }
}

KDCoordinate
FunctionGraphController::FunctionSelectionController::nonMemoizedRowHeight(
    int row) {
  assert(row < graphController()->numberOfCurves());
  ExpiringPointer<Function> function =
      graphController()->functionStore()->modelForRecord(
          graphController()->recordAtCurveIndex(row));
  return std::max(function->layout().layoutSize(k_font).height(),
                  nameLayoutAtIndex(row).layoutSize(k_font).height()) +
         Metric::CellTopMargin + Metric::CellBottomMargin +
         Metric::CellSeparatorThickness;
}

void FunctionGraphController::FunctionSelectionController::fillCellForRow(
    HighlightCell *cell, int row) {
  assert(row < graphController()->numberOfCurves());
  ExpiringPointer<Function> function =
      graphController()->functionStore()->modelForRecord(
          graphController()->recordAtCurveIndex(row));
  static_cast<CurveSelectionCellWithChevron *>(cell)->setColor(
      function->color());
  static_cast<CurveSelectionCellWithChevron *>(cell)->label()->setLayout(
      function->layout().clone());
}

void FunctionGraphController::FunctionSelectionController::
    didBecomeFirstResponder() {
  if (numberOfRows() <= 1) {
    /* This can happen if all functions were deactivated within the calculate
     * menu. The function selection menu is still on the stack but it's now
     * empty (or has only 1 function, in which case it should not appear.) */
    static_cast<StackViewController *>(parentResponder())->pop();
    return;
  }
  CurveSelectionController::didBecomeFirstResponder();
}

void FunctionGraphController::reloadBannerView() {
  assert(numberOfCurves() > 0);
  Ion::Storage::Record record = recordAtSelectedCurveIndex();
  reloadBannerViewForCursorOnFunction(
      m_cursor, record, functionStore(),
      AppsContainerHelper::sharedAppsContainerGlobalContext());
}

double FunctionGraphController::defaultCursorT(Ion::Storage::Record record,
                                               bool ignoreMargins) {
  Poincare::Context *context = textFieldDelegateApp()->localContext();
  ExpiringPointer<Function> function = functionStore()->modelForRecord(record);
  float gridUnit = 2.0 * interactiveCurveViewRange()->xGridUnit();
  float xMin = interactiveCurveViewRange()->xMin();
  float xMax = interactiveCurveViewRange()->xMax();

  float middle = (interactiveCurveViewRange()->xMin() +
                  interactiveCurveViewRange()->xMax()) /
                 2.0f;
  middle = gridUnit * std::floor(middle / gridUnit);
  float currentX, currentY;
  int iterations = 0;
  do {
    /* Start from the middle and place the cursor on each grid unit until you
     * find an x where the cursor is visible.
     * currentX values will alternate over and under the middle value.
     * Example: If the middle is 20 and grid unit is 5, cursorX values
     * will be 20/25/15/30/10/35/05/40/00 etc. */
    currentX = middle + (iterations % 2 == 0 ? -1 : 1) *
                            ((iterations + 1) / 2) * gridUnit;
    // Using first subCurve for default cursor.
    currentY = function->evaluateXYAtParameter(currentX, context, 0).y();
    iterations++;
  } while (xMin < currentX && currentX < xMax &&
           !isCursorVisibleAtPosition(Coordinate2D<float>(currentX, currentY),
                                      ignoreMargins));

  if (!isCursorVisibleAtPosition(Coordinate2D<float>(currentX, currentY),
                                 ignoreMargins)) {
    // If no positions make the cursor visible, return the middle value
    currentX = middle;
  }
  return currentX;
}

FunctionStore *FunctionGraphController::functionStore() const {
  return FunctionApp::app()->functionStore();
}

void FunctionGraphController::computeDefaultPositionForFunctionAtIndex(
    int index, double *t, Coordinate2D<double> *xy, bool ignoreMargins) {
  Ion::Storage::Record record = recordAtCurveIndex(index);
  ExpiringPointer<Function> function = functionStore()->modelForRecord(record);
  *t = defaultCursorT(record, ignoreMargins);
  *xy = function->evaluateXYAtParameter(
      *t, textFieldDelegateApp()->localContext(), 0);
}

void FunctionGraphController::initCursorParameters(bool ignoreMargins) {
  const int activeFunctionsCount = numberOfCurves();
  assert(activeFunctionsCount > 0);
  int functionIndex = 0;
  m_selectedSubCurveIndex = 0;
  Coordinate2D<double> xy;
  double t;

  do {
    computeDefaultPositionForFunctionAtIndex(functionIndex, &t, &xy,
                                             ignoreMargins);
  } while (!isCursorVisibleAtPosition(xy, ignoreMargins) &&
           ++functionIndex < activeFunctionsCount);

  if (functionIndex == activeFunctionsCount) {
    functionIndex = 0;
    computeDefaultPositionForFunctionAtIndex(functionIndex, &t, &xy,
                                             ignoreMargins);
  }

  m_cursor->moveTo(t, xy.x(), xy.y());
  selectCurveAtIndex(functionIndex, false);
}

bool FunctionGraphController::moveCursorVertically(
    OMG::VerticalDirection direction) {
  int currentActiveFunctionIndex = *m_selectedCurveIndex;
  Poincare::Context *context = textFieldDelegateApp()->localContext();
  int nextSubCurve = 0;
  int nextFunction =
      nextCurveIndexVertically(direction, currentActiveFunctionIndex, context,
                               m_selectedSubCurveIndex, &nextSubCurve);
  if (nextFunction < 0) {
    return false;
  }

  moveCursorVerticallyToPosition(nextFunction, nextSubCurve, m_cursor->t());
  return true;
}

void FunctionGraphController::moveCursorVerticallyToPosition(int nextFunction,
                                                             int nextSubCurve,
                                                             double nextT) {
  // Clip the current t to the domain of the next function
  ExpiringPointer<Function> f =
      functionStore()->modelForRecord(recordAtCurveIndex(nextFunction));
  if (!std::isnan(f->tMin())) {
    assert(!std::isnan(f->tMax()));
    nextT = std::min<double>(f->tMax(), std::max<double>(f->tMin(), nextT));
  }
  Poincare::Context *context = textFieldDelegateApp()->localContext();
  Poincare::Coordinate2D<double> cursorPosition =
      f->evaluateXYAtParameter(nextT, context, nextSubCurve);
  m_cursor->moveTo(nextT, cursorPosition.x(), cursorPosition.y());
  selectCurveAtIndex(nextFunction, true);
  // Prevent the abscissaValue from edition if the function is along y
  Escher::Responder *responder = isAlongY(*m_selectedCurveIndex)
                                     ? static_cast<Responder *>(this)
                                     : bannerView()->abscissaValue();
  if (Container::activeApp()->firstResponder() != responder) {
    Container::activeApp()->setFirstResponder(responder);
  }
  m_selectedSubCurveIndex = nextSubCurve;
}

bool FunctionGraphController::selectedModelIsValid() const {
  int curveIndex = *m_selectedCurveIndex;
  return curveIndex < numberOfCurves() &&
         m_selectedSubCurveIndex < numberOfSubCurves(curveIndex);
}

Poincare::Coordinate2D<double> FunctionGraphController::selectedModelXyValues(
    double t) const {
  assert(selectedModelIsValid());
  return xyValues(*m_selectedCurveIndex, t,
                  textFieldDelegateApp()->localContext(),
                  m_selectedSubCurveIndex);
}

AbstractPlotView *FunctionGraphController::curveView() {
  return functionGraphView();
}

Coordinate2D<double> FunctionGraphController::xyValues(
    int curveIndex, double t, Poincare::Context *context,
    int subCurveIndex) const {
  return functionStore()
      ->modelForRecord(recordAtCurveIndex(curveIndex))
      ->evaluateXYAtParameter(t, context, subCurveIndex);
}

int FunctionGraphController::numberOfSubCurves(int curveIndex) const {
  return functionStore()
      ->modelForRecord(recordAtCurveIndex(curveIndex))
      ->numberOfSubCurves();
}

bool FunctionGraphController::isAlongY(int curveIndex) const {
  return functionStore()
      ->modelForRecord(recordAtCurveIndex(curveIndex))
      ->isAlongY();
}

int FunctionGraphController::numberOfCurves() const {
  return functionStore()->numberOfActiveFunctions();
}

void FunctionGraphController::tidyModels(Poincare::TreeNode *treePoolCursor) {
  int nbOfFunctions = numberOfCurves();
  for (int i = 0; i < nbOfFunctions; i++) {
    ExpiringPointer<Function> f =
        functionStore()->modelForRecord(recordAtCurveIndex(i));
    f->tidyDownstreamPoolFrom(treePoolCursor);
  }
}

}  // namespace Shared
