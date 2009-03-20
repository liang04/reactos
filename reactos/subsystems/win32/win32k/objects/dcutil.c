
#include <w32k.h>

#define NDEBUG
#include <debug.h>

/*
 * DC device-independent Get/SetXXX functions
 * (RJJ) swiped from WINE
 */

#define DC_GET_VAL( func_type, func_name, dc_field ) \
func_type APIENTRY  func_name( HDC hdc ) \
{                                   \
  func_type  ft;                    \
  PDC  dc = DC_LockDc( hdc );       \
  PDC_ATTR pdcattr;                 \
  if (!dc)                          \
  {                                 \
    SetLastWin32Error(ERROR_INVALID_HANDLE); \
    return 0;                       \
  }                                 \
  pdcattr = dc->pdcattr;           \
  ft = pdcattr->dc_field;           \
  DC_UnlockDc(dc);                  \
  return ft;                        \
}

/* DC_GET_VAL_EX is used to define functions returning a POINT or a SIZE. It is
 * important that the function has the right signature, for the implementation
 * we can do whatever we want.
 */
#define DC_GET_VAL_EX( FuncName, ret_x, ret_y, type, ax, ay ) \
VOID FASTCALL Int##FuncName ( PDC dc, LP##type pt) \
{ \
  PDC_ATTR pdcattr; \
  ASSERT(dc); \
  ASSERT(pt); \
  pdcattr = dc->pdcattr; \
  pt->ax = pdcattr->ret_x; \
  pt->ay = pdcattr->ret_y; \
}

#if 0
BOOL APIENTRY NtGdi##FuncName ( HDC hdc, LP##type pt ) \
{ \
  NTSTATUS Status = STATUS_SUCCESS; \
  type Safept; \
  PDC dc; \
  if(!pt) \
  { \
    SetLastWin32Error(ERROR_INVALID_PARAMETER); \
    return FALSE; \
  } \
  if(!(dc = DC_LockDc(hdc))) \
  { \
    SetLastWin32Error(ERROR_INVALID_HANDLE); \
    return FALSE; \
  } \
  Int##FuncName( dc, &Safept); \
  DC_UnlockDc(dc); \
  _SEH2_TRY \
  { \
    ProbeForWrite(pt, \
                  sizeof( type ), \
                  1); \
    *pt = Safept; \
  } \
  _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER) \
  { \
    Status = _SEH2_GetExceptionCode(); \
  } \
  _SEH2_END; \
  if(!NT_SUCCESS(Status)) \
  { \
    SetLastNtError(Status); \
    return FALSE; \
  } \
  return TRUE; \
}
#endif

#define DC_SET_MODE( func_name, dc_field, min_val, max_val ) \
INT APIENTRY  func_name( HDC hdc, INT mode ) \
{                                           \
  INT  prevMode;                            \
  PDC  dc;                                  \
  PDC_ATTR pdcattr;                         \
  if ((mode < min_val) || (mode > max_val)) \
  { \
    SetLastWin32Error(ERROR_INVALID_PARAMETER); \
    return 0;                               \
  } \
  dc = DC_LockDc ( hdc );              \
  if ( !dc )                                \
  { \
    SetLastWin32Error(ERROR_INVALID_HANDLE); \
    return 0;                               \
  } \
  pdcattr = dc->pdcattr;           \
  prevMode = pdcattr->dc_field;             \
  pdcattr->dc_field = mode;                 \
  DC_UnlockDc ( dc );                    \
  return prevMode;                          \
}


static
VOID
CopytoUserDcAttr(PDC dc, PDC_ATTR pdcattr)
{
  NTSTATUS Status = STATUS_SUCCESS;
  dc->Dc_Attr.mxWorldToDevice = dc->DcLevel.mxWorldToDevice;
  dc->Dc_Attr.mxDeviceToWorld = dc->DcLevel.mxDeviceToWorld;
  dc->Dc_Attr.mxWorldToPage = dc->DcLevel.mxWorldToPage;

  _SEH2_TRY
  {
      ProbeForWrite( pdcattr,
             sizeof(DC_ATTR),
                           1);
      RtlCopyMemory( pdcattr,
                &dc->Dc_Attr,
             sizeof(DC_ATTR));
  }
  _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
  {
     Status = _SEH2_GetExceptionCode();
     ASSERT(FALSE);
  }
  _SEH2_END;
}


BOOL
FASTCALL
DCU_SyncDcAttrtoUser(PDC dc)
{
  PDC_ATTR pdcattr = dc->pdcattr;

  if (pdcattr == &dc->Dc_Attr) return TRUE; // No need to copy self.
  ASSERT(pdcattr);
  CopytoUserDcAttr( dc, pdcattr);
  return TRUE;
}

BOOL
FASTCALL
DCU_SynchDcAttrtoUser(HDC hDC)
{
  BOOL Ret;
  PDC pDC = DC_LockDc ( hDC );
  if (!pDC) return FALSE;
  Ret = DCU_SyncDcAttrtoUser(pDC);
  DC_UnlockDc( pDC );
  return Ret;
}


DC_GET_VAL( INT, IntGdiGetMapMode, iMapMode )
DC_GET_VAL( INT, IntGdiGetPolyFillMode, jFillMode )
DC_GET_VAL( COLORREF, IntGdiGetBkColor, crBackgroundClr )
DC_GET_VAL( INT, IntGdiGetBkMode, jBkMode )
DC_GET_VAL( INT, IntGdiGetROP2, jROP2 )
DC_GET_VAL( INT, IntGdiGetStretchBltMode, jStretchBltMode )
DC_GET_VAL( UINT, IntGdiGetTextAlign, lTextAlign )
DC_GET_VAL( COLORREF, IntGdiGetTextColor, crForegroundClr )

DC_GET_VAL_EX( GetViewportOrgEx, ptlViewportOrg.x, ptlViewportOrg.y, POINT, x, y )
DC_GET_VAL_EX( GetWindowExtEx, szlWindowExt.cx, szlWindowExt.cy, SIZE, cx, cy )
DC_GET_VAL_EX( GetWindowOrgEx, ptlWindowOrg.x, ptlWindowOrg.y, POINT, x, y )

DC_SET_MODE( IntGdiSetPolyFillMode, jFillMode, ALTERNATE, WINDING )
DC_SET_MODE( IntGdiSetROP2, jROP2, R2_BLACK, R2_WHITE )
DC_SET_MODE( IntGdiSetStretchBltMode, jStretchBltMode, BLACKONWHITE, HALFTONE )



COLORREF FASTCALL
IntGdiSetBkColor(HDC hDC, COLORREF color)
{
  COLORREF oldColor;
  PDC dc;
  PDC_ATTR pdcattr;
  HBRUSH hBrush;

  if (!(dc = DC_LockDc(hDC)))
  {
    SetLastWin32Error(ERROR_INVALID_HANDLE);
    return CLR_INVALID;
  }
  pdcattr = dc->pdcattr;
  oldColor = pdcattr->crBackgroundClr;
  pdcattr->crBackgroundClr = color;
  pdcattr->ulBackgroundClr = (ULONG)color;
  pdcattr->ulDirty_ &= ~(DIRTY_BACKGROUND|DIRTY_LINE|DIRTY_FILL); // Clear Flag if set.
  hBrush = pdcattr->hbrush;
  DC_UnlockDc(dc);
  NtGdiSelectBrush(hDC, hBrush);
  return oldColor;
}

INT FASTCALL
IntGdiSetBkMode(HDC hDC, INT Mode)
{
  COLORREF oldMode;
  PDC dc;
  PDC_ATTR pdcattr;

  if (!(dc = DC_LockDc(hDC)))
  {
    SetLastWin32Error(ERROR_INVALID_HANDLE);
    return CLR_INVALID;
  }
  pdcattr = dc->pdcattr;
  oldMode = pdcattr->lBkMode;
  pdcattr->jBkMode = Mode;
  pdcattr->lBkMode = Mode;
  DC_UnlockDc(dc);
  return oldMode;
}

UINT
FASTCALL
IntGdiSetTextAlign(HDC  hDC,
                       UINT  Mode)
{
  UINT prevAlign;
  DC *dc;
  PDC_ATTR pdcattr;

  dc = DC_LockDc(hDC);
  if (!dc)
    {
      SetLastWin32Error(ERROR_INVALID_HANDLE);
      return GDI_ERROR;
    }
  pdcattr = dc->pdcattr;
  prevAlign = pdcattr->lTextAlign;
  pdcattr->lTextAlign = Mode;
  DC_UnlockDc( dc );
  return  prevAlign;
}

COLORREF
FASTCALL
IntGdiSetTextColor(HDC hDC,
                 COLORREF color)
{
  COLORREF  oldColor;
  PDC  dc = DC_LockDc(hDC);
  PDC_ATTR pdcattr;
  HBRUSH hBrush;

  if (!dc)
  {
    SetLastWin32Error(ERROR_INVALID_HANDLE);
    return CLR_INVALID;
  }
  pdcattr = dc->pdcattr;

  oldColor = pdcattr->crForegroundClr;
  pdcattr->crForegroundClr = color;
  hBrush = pdcattr->hbrush;
  pdcattr->ulDirty_ &= ~(DIRTY_TEXT|DIRTY_LINE|DIRTY_FILL);
  DC_UnlockDc( dc );
  NtGdiSelectBrush(hDC, hBrush);
  return  oldColor;
}

VOID
FASTCALL
DCU_SetDcUndeletable(HDC  hDC)
{
  PDC dc = DC_LockDc(hDC);
  if (!dc)
    {
      SetLastWin32Error(ERROR_INVALID_HANDLE);
      return;
    }

  dc->fs |= DC_FLAG_PERMANENT;
  DC_UnlockDc( dc );
  return;
}
