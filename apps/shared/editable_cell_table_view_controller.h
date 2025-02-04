#ifndef SHARED_EDITABLE_CELL_TABLE_VIEW_CONTROLLER_H
#define SHARED_EDITABLE_CELL_TABLE_VIEW_CONTROLLER_H

#include <escher/even_odd_buffer_text_cell.h>
#include <escher/regular_table_view_data_source.h>
#include <escher/stack_view_controller.h>
#include <poincare/preferences.h>

#include "column_helper.h"
#include "column_parameter_controller.h"
#include "editable_cell_selectable_table_view.h"
#include "tab_table_controller.h"
#include "text_field_delegate.h"

namespace Shared {

class ColumnParameterController;

class EditableCellTableViewController : public TabTableController,
                                        public Escher::TableViewDataSource,
                                        public TextFieldDelegate,
                                        public ClearColumnHelper {
 public:
  EditableCellTableViewController(
      Responder* parentResponder,
      Escher::SelectableTableViewDelegate* delegate = nullptr);
  bool textFieldShouldFinishEditing(Escher::AbstractTextField* textField,
                                    Ion::Events::Event event) override;
  bool textFieldDidFinishEditing(Escher::AbstractTextField* textField,
                                 const char* text,
                                 Ion::Events::Event event) override;

  int numberOfRows() const override;
  void fillCellForLocationWithDisplayMode(
      Escher::HighlightCell* cell, int column, int row,
      Poincare::Preferences::PrintFloatMode mode);
  void viewWillAppear() override;
  void didBecomeFirstResponder() override;

  bool handleEvent(Ion::Events::Event event) override;

  virtual int numberOfRowsAtColumn(int i) const = 0;

 protected:
  constexpr static KDFont::Size k_cellFont = KDFont::Size::Small;
  constexpr static KDCoordinate k_cellHeight =
      Escher::Metric::SmallEditableCellHeight;
  constexpr static KDCoordinate k_cellWidth =
      Poincare::PrintFloat::glyphLengthForFloatWithPrecision(
          Escher::AbstractEvenOddBufferTextCell::k_defaultPrecision) *
          KDFont::GlyphWidth(k_cellFont) +
      2 * Escher::Metric::SmallCellMargin;
  constexpr static KDCoordinate k_margin =
      Escher::Metric::TableSeparatorThickness;
  constexpr static KDCoordinate k_scrollBarMargin =
      Escher::Metric::CommonRightMargin;

  constexpr static int k_maxNumberOfDisplayableRows =
      Escher::Metric::MinimalNumberOfScrollableRowsToFillDisplayHeight(
          k_cellHeight, Escher::Metric::TabHeight);
  constexpr static int k_maxNumberOfDisplayableColumns =
      Ion::Display::Width / k_cellWidth + 2;
  constexpr static int k_maxNumberOfDisplayableCells =
      k_maxNumberOfDisplayableRows * k_maxNumberOfDisplayableColumns;

  // TabTableController
  Escher::SelectableTableView* selectableTableView() override {
    return &m_selectableTableView;
  }

  // TableViewDataSource
  KDCoordinate defaultRowHeight() override { return k_cellHeight; }
  KDCoordinate defaultColumnWidth() override { return k_cellWidth; }

  virtual void updateSizeMemoizationForRow(int row,
                                           KDCoordinate rowPreviousHeight) {}

  // ClearColumnHelper
  Escher::SelectableTableView* table() override {
    return selectableTableView();
  }

  /* Poor's man diamond inheritance */
  virtual Escher::SelectableViewController* columnParameterController() = 0;
  virtual Shared::ColumnParameters* columnParameters() = 0;
  virtual Escher::StackViewController* stackController() const = 0;
  virtual void setTitleCellText(Escher::HighlightCell* cell, int column) = 0;
  virtual void setTitleCellStyle(Escher::HighlightCell* cell, int column) = 0;
  virtual void reloadEditedCell(int column, int row) {
    selectableTableView()->reloadCellAtLocation(column, row);
  }
  virtual bool checkDataAtLocation(double floatBody, int column,
                                   int row) const {
    return true;
  }

  EditableCellSelectableTableView m_selectableTableView;

 private:
  virtual void didChangeCell(int column, int row) {}
  virtual bool cellAtLocationIsEditable(int column, int row) = 0;
  virtual bool setDataAtLocation(double floatBody, int column, int row) = 0;
  virtual double dataAtLocation(int column, int row) = 0;
  virtual int maxNumberOfElements() const = 0;
};

}  // namespace Shared

#endif
