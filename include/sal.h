// sal.h: the marks of Microsoft's source annotations (_In_, _Out_, _Inout_opt_...).
// They tell a code checker what a parameter is for; the compiler has nothing to
// do with them, so here every one of them is empty. windows.h includes this file.
#pragma once

#define _In_
#define _In_opt_
#define _In_z_
#define _In_opt_z_
#define _In_reads_(n)
#define _In_reads_opt_(n)
#define _In_reads_bytes_(n)
#define _In_reads_bytes_opt_(n)
#define _In_reads_z_(n)
#define _In_reads_or_z_(n)
#define _In_range_(a, b)
#define _Out_
#define _Out_opt_
#define _Out_writes_(n)
#define _Out_writes_opt_(n)
#define _Out_writes_bytes_(n)
#define _Out_writes_bytes_opt_(n)
#define _Out_writes_z_(n)
#define _Out_writes_to_(n, m)
#define _Out_writes_to_opt_(n, m)
#define _Out_writes_bytes_to_(n, m)
#define _Out_writes_bytes_to_opt_(n, m)
#define _Out_writes_all_(n)
#define _Out_range_(a, b)
#define _Inout_
#define _Inout_opt_
#define _Inout_z_
#define _Inout_opt_z_
#define _Inout_updates_(n)
#define _Inout_updates_opt_(n)
#define _Inout_updates_bytes_(n)
#define _Inout_updates_bytes_opt_(n)
#define _Inout_updates_z_(n)
#define _Outptr_
#define _Outptr_opt_
#define _Outptr_result_maybenull_
#define _Outptr_opt_result_maybenull_
#define _Outptr_result_buffer_(n)
#define _Outptr_result_bytebuffer_(n)
#define _Outptr_result_z_
#define _COM_Outptr_
#define _COM_Outptr_opt_
#define _Deref_out_
#define _Deref_out_opt_
#define _Deref_opt_out_
#define _Deref_out_z_
#define _Ret_
#define _Ret_z_
#define _Ret_opt_
#define _Ret_maybenull_
#define _Ret_notnull_
#define _Ret_writes_(n)
#define _Ret_writes_bytes_(n)
#define _Ret_writes_maybenull_(n)
#define _Ret_range_(a, b)
#define _Result_nullonfailure_
#define _Result_zeroonfailure_
#define _Check_return_
#define _Must_inspect_result_
#define _Success_(e)
#define _Return_type_success_(e)
#define _On_failure_(a)
#define _Always_(a)
#define _Use_decl_annotations_
#define _Printf_format_string_
#define _Scanf_format_string_
#define _Scanf_s_format_string_
#define _Format_string_impl_(a, b)
#define _Null_terminated_
#define _NullNull_terminated_
#define _Notnull_
#define _Maybenull_
#define _Null_
#define _Pre_
#define _Post_
#define _Pre_z_
#define _Post_z_
#define _Pre_notnull_
#define _Pre_maybenull_
#define _Pre_null_
#define _Post_invalid_
#define _Post_valid_
#define _Pre_valid_
#define _Pre_opt_valid_
#define _Pre_readable_size_(n)
#define _Pre_writable_size_(n)
#define _Post_readable_size_(n)
#define _Post_writable_size_(n)
#define _Post_satisfies_(e)
#define _Pre_satisfies_(e)
#define _Field_size_(n)
#define _Field_size_opt_(n)
#define _Field_size_bytes_(n)
#define _Field_size_bytes_opt_(n)
#define _Field_size_part_(n, m)
#define _Field_size_bytes_part_(n, m)
#define _Field_z_
#define _Field_range_(a, b)
#define _Struct_size_bytes_(n)
#define _Frees_ptr_
#define _Frees_ptr_opt_
#define _Post_ptr_invalid_
#define _Reserved_
#define _Const_
#define _Analysis_assume_(e)
#define _Analysis_noreturn_
#define _When_(a, b)
#define _At_(a, b)
#define _At_buffer_(a, b, c, d)
#define _Acquires_lock_(l)
#define _Releases_lock_(l)
#define _Acquires_exclusive_lock_(l)
#define _Releases_exclusive_lock_(l)
#define _Acquires_shared_lock_(l)
#define _Releases_shared_lock_(l)
#define _Requires_lock_held_(l)
#define _Requires_lock_not_held_(l)
#define _Requires_exclusive_lock_held_(l)
#define _Requires_shared_lock_held_(l)
#define _Requires_no_locks_held_
#define _Guarded_by_(l)
#define _Interlocked_
#define _Interlocked_operand_
#define _Function_class_(n)
#define _IRQL_requires_(n)
#define _IRQL_requires_max_(n)
#define _IRQL_raises_(n)
#define _IRQL_saves_
#define _IRQL_restores_
#define _Kernel_requires_resource_held_(r)
#define _Callback_
#define _Dispatch_type_(t)
#define _Strict_type_match_
#define _Points_to_data_
#define _Literal_
#define _Notliteral_
#define _Unchanged_(e)
#define _Writable_elements_(n)
#define _Writable_bytes_(n)
#define _Readable_elements_(n)
#define _Readable_bytes_(n)
#define _Valid_
#define _Notvalid_
#define _Maybevalid_
#define _In_count_(n)
#define _Out_cap_(n)
#define _Out_bytecap_(n)
#define _Inout_cap_(n)
#define _Deref_post_z_
#define __in
#define __in_opt
#define __out
#define __out_opt
#define __inout
#define __inout_opt
#define __reserved
#define __checkReturn
#define __success(e)
