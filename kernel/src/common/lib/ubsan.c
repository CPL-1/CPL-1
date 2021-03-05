#include <common/lib/kmsg.h>
#include <common/lib/ubsan.h>

// TODO: use proper printf specifiers

void UBSan_PanicAt(struct UBSan_SourceLocation *location, const char *err) {
	if (location != NULL) {
		KernelLog_ErrorMsg("UB Sanitizer", "Error at %s:%d:%d: \"%s\"", location->file, location->line,
						   location->column, err);
	} else {
		KernelLog_ErrorMsg("UB Sanitizer", "Error: \"%s\"", err);
	}
}

void __ubsan_handle_type_mismatch_v1(struct UBSan_MismatchData *data, uintptr_t ptr) {
	if (ptr == 0) {
		UBSan_PanicAt(&data->location, "NULL dereference");
	} else if (data->align != 0 && ALIGN_UP(ptr, 1 << data->align) != ptr) {
		printf("Pointer %p is not aligned to %d", ptr, 1 << data->align);
		UBSan_PanicAt(&data->location, "Unaligned pointer");
	} else {
		printf("Pointer %p points to a cell that is smaller than need to store \"%s\"", ptr, data->type->name);
		UBSan_PanicAt(&data->location, "Small pointer target size");
	}
}

void __ubsan_handle_add_overflow() {
	UBSan_PanicAt(NULL, "Addition overflow");
}

void __ubsan_handle_sub_overflow() {
	UBSan_PanicAt(NULL, "Substraction overflow");
}

void __ubsan_handle_mul_overflow() {
	UBSan_PanicAt(NULL, "Multiplication overflow");
}

void __ubsan_handle_divrem_overflow() {
	UBSan_PanicAt(NULL, "Division overflow");
}

void __ubsan_handle_negate_overflow() {
	UBSan_PanicAt(NULL, "Negate overflow");
}

void __ubsan_handle_pointer_overflow(void *dataRaw, void *lhs, void *rhs) {
	struct UBSan_OverflowData *data = (struct UBSan_OverflowData *)dataRaw;
	printf("Pointer overflow with operands %p, %p", lhs, rhs);
	UBSan_PanicAt(&data->location, "Pointer overflow");
}

void __ubsan_handle_out_of_bounds(void *dataRaw, void *index) {
	struct UBSan_OutOfBoundsData *data = (struct UBSan_OutOfBoundsData *)dataRaw;
	printf("Out of bounds at index %d", (intptr_t)index);
	UBSan_PanicAt(&data->location, "out of bounds");
}

void __ubsan_handle_shift_out_of_bounds(void *dataRaw) {
	struct UBSan_ShiftOutOfBoundsData *data = (struct UBSan_ShiftOutOfBoundsData *)dataRaw;
	UBSan_PanicAt(&data->location, "Shift out of bounds");
}

void __ubsan_handle_load_invalid_value() {
	UBSan_PanicAt(NULL, "Invalid value load");
}

void __ubsan_handle_float_cast_overflow() {
	UBSan_PanicAt(NULL, "Float cast overflow");
}

void __ubsan_handle_builtin_unreachable() {
	UBSan_PanicAt(NULL, "Unreachable reached");
}
