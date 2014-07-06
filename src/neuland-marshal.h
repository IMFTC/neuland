
#ifndef __neuland_marshal_MARSHAL_H__
#define __neuland_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:OBJECT,STRING (neuland-marshal.list:1) */
extern void neuland_marshal_VOID__OBJECT_STRING (GClosure     *closure,
                                                 GValue       *return_value,
                                                 guint         n_param_values,
                                                 const GValue *param_values,
                                                 gpointer      invocation_hint,
                                                 gpointer      marshal_data);

/* VOID:OBJECT,STRING,STRING (neuland-marshal.list:2) */
extern void neuland_marshal_VOID__OBJECT_STRING_STRING (GClosure     *closure,
                                                        GValue       *return_value,
                                                        guint         n_param_values,
                                                        const GValue *param_values,
                                                        gpointer      invocation_hint,
                                                        gpointer      marshal_data);

G_END_DECLS

#endif /* __neuland_marshal_MARSHAL_H__ */
