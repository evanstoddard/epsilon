#include "interval_parameter_controller.h"

using namespace Escher;

namespace Shared {

Interval::IntervalParameters *
IntervalParameterController::SharedTempIntervalParameters() {
  static Interval::IntervalParameters sTempIntervalParameters;
  return &sTempIntervalParameters;
}

IntervalParameterController::IntervalParameterController(
    Responder *parentResponder,
    InputEventHandlerDelegate *inputEventHandlerDelegate)
    : FloatParameterController<double>(parentResponder),
      m_interval(nullptr),
      m_title(I18n::Message::IntervalSet),
      m_startMessage(I18n::Message::XStart),
      m_endMessage(I18n::Message::XEnd),
      m_confirmPopUpController(Invocation::Builder<IntervalParameterController>(
          [](IntervalParameterController *controller, void *sender) {
            controller->stackController()->pop();
            return true;
          },
          this)) {
  for (int i = 0; i < k_totalNumberOfCell; i++) {
    m_intervalCells[i].setParentResponder(&m_selectableListView);
    m_intervalCells[i].setDelegates(inputEventHandlerDelegate, this);
  }
}

void IntervalParameterController::setInterval(Interval *interval) {
  m_interval = interval;
  *SharedTempIntervalParameters() = *(interval->parameters());
}

const char *IntervalParameterController::title() {
  return I18n::translate(m_title);
}

int IntervalParameterController::numberOfRows() const {
  return k_totalNumberOfCell + 1;
}

void IntervalParameterController::setStartEndMessages(
    I18n::Message startMessage, I18n::Message endMessage) {
  m_startMessage = startMessage;
  m_endMessage = endMessage;
}

void IntervalParameterController::fillCellForRow(HighlightCell *cell, int row) {
  if (row == numberOfRows() - 1) {
    return;
  }

  MenuCellWithEditableText<MessageTextView> *myCell =
      static_cast<MenuCellWithEditableText<MessageTextView> *>(cell);
  assert(row >= 0 && row < 3);
  I18n::Message m = row == 0 ? m_startMessage
                             : (row == 1 ? m_endMessage : I18n::Message::Step);
  myCell->label()->setMessage(m);
  FloatParameterController::fillCellForRow(cell, row);
}

double IntervalParameterController::parameterAtIndex(int index) {
  GetterPointer getters[k_totalNumberOfCell] = {
      &Interval::IntervalParameters::start, &Interval::IntervalParameters::end,
      &Interval::IntervalParameters::step};
  return (SharedTempIntervalParameters()->*getters[index])();
}

bool IntervalParameterController::setParameterAtIndex(int parameterIndex,
                                                      double f) {
  if (f <= 0.0f && parameterIndex == 2) {
    Container::activeApp()->displayWarning(I18n::Message::ForbiddenValue);
    return false;
  }
  double start =
      parameterIndex == 0 ? f : SharedTempIntervalParameters()->start();
  double end = parameterIndex == 1 ? f : SharedTempIntervalParameters()->end();
  if (start > end) {
    if (parameterIndex == 1) {
      Container::activeApp()->displayWarning(I18n::Message::ForbiddenValue);
      return false;
    }
    double g = f + 1.0;
    SharedTempIntervalParameters()->setEnd(g);
  }
  SetterPointer setters[k_totalNumberOfCell] = {
      &Interval::IntervalParameters::setStart,
      &Interval::IntervalParameters::setEnd,
      &Interval::IntervalParameters::setStep};
  (SharedTempIntervalParameters()->*setters[parameterIndex])(f);
  return true;
}

HighlightCell *IntervalParameterController::reusableParameterCell(int index,
                                                                  int type) {
  assert(index >= 0);
  assert(index < k_totalNumberOfCell);
  return &m_intervalCells[index];
}

TextField *IntervalParameterController::textFieldOfCellAtIndex(
    HighlightCell *cell, int index) {
  assert(typeAtRow(index) == k_parameterCellType);
  return static_cast<MenuCellWithEditableText<MessageTextView> *>(cell)
      ->textField();
}

bool IntervalParameterController::handleEvent(Ion::Events::Event event) {
  if (event == Ion::Events::Left && stackController()->depth() > 2) {
    stackController()->pop();
    return true;
  }
  if (event == Ion::Events::Back &&
      !m_interval->hasSameParameters(*SharedTempIntervalParameters())) {
    // Open pop-up to confirm discarding values
    m_confirmPopUpController.presentModally();
    return true;
  }
  return false;
}

int IntervalParameterController::reusableParameterCellCount(int type) {
  return k_totalNumberOfCell;
}

void IntervalParameterController::buttonAction() {
  m_interval->setParameters(*SharedTempIntervalParameters());
  m_interval->forceRecompute();
  StackViewController *stack = stackController();
  stack->popUntilDepth(1, true);
}

}  // namespace Shared
