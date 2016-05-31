/* $Id: ps_add_to_collection_request.c,v 1.0 2005/10/14 15:17:55 fpeters Exp $
 *
 * Lasso - A free implementation of the Liberty Alliance specifications.
 *
 * Copyright (C) 2004-2007 Entr'ouvert
 * http://lasso.entrouvert.org
 *
 * Authors: See AUTHORS file in top-level directory.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../private.h"
#include "ps_add_to_collection_request.h"
#include "./idwsf2_strings.h"

/**
 * SECTION:ps_add_to_collection_request
 * @short_description: &lt;ps:AddToCollectionRequest&gt;
 *
 * <figure><title>Schema fragment for ps:AddToCollectionRequest</title>
 * <programlisting><![CDATA[
 *
 * <xs:complexType name="AddToCollectionRequestType">
 *   <xs:complexContent>
 *     <xs:extension base="RequestAbstractType">
 *       <xs:sequence>
 *         <xs:element ref="TargetObjectID"/>
 *         <xs:element ref="ObjectID" minOccurs="1" maxOccurs="unbounded"/>
 *         <xs:element ref="Subscription" minOccurs="0"/>
 *       </xs:sequence>
 *     </xs:extension>
 *   </xs:complexContent>
 * </xs:complexType>
 * ]]></programlisting>
 * </figure>
 */

/*****************************************************************************/
/* private methods                                                           */
/*****************************************************************************/


static struct XmlSnippet schema_snippets[] = {
	{ "TargetObjectID", SNIPPET_NODE,
		G_STRUCT_OFFSET(LassoIdWsf2PsAddToCollectionRequest, TargetObjectID), NULL, NULL, NULL},
	{ "ObjectID", SNIPPET_LIST_NODES,
		G_STRUCT_OFFSET(LassoIdWsf2PsAddToCollectionRequest, ObjectID), NULL, NULL, NULL},
	{ "Subscription", SNIPPET_NODE,
		G_STRUCT_OFFSET(LassoIdWsf2PsAddToCollectionRequest, Subscription), NULL, NULL, NULL},
	{NULL, 0, 0, NULL, NULL, NULL}
};

static LassoNodeClass *parent_class = NULL;


/*****************************************************************************/
/* instance and class init functions                                         */
/*****************************************************************************/


static void
class_init(LassoIdWsf2PsAddToCollectionRequestClass *klass)
{
	LassoNodeClass *nclass = LASSO_NODE_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	nclass->node_data = g_new0(LassoNodeClassData, 1);
	lasso_node_class_set_nodename(nclass, "AddToCollectionRequest");
	lasso_node_class_set_ns(nclass, LASSO_IDWSF2_PS_HREF, LASSO_IDWSF2_PS_PREFIX);
	lasso_node_class_add_snippets(nclass, schema_snippets);
}

GType
lasso_idwsf2_ps_add_to_collection_request_get_type()
{
	static GType this_type = 0;

	if (!this_type) {
		static const GTypeInfo this_info = {
			sizeof (LassoIdWsf2PsAddToCollectionRequestClass),
			NULL,
			NULL,
			(GClassInitFunc) class_init,
			NULL,
			NULL,
			sizeof(LassoIdWsf2PsAddToCollectionRequest),
			0,
			NULL,
			NULL
		};

		this_type = g_type_register_static(LASSO_TYPE_IDWSF2_PS_REQUEST_ABSTRACT,
				"LassoIdWsf2PsAddToCollectionRequest", &this_info, 0);
	}
	return this_type;
}

/**
 * lasso_idwsf2_ps_add_to_collection_request_new:
 *
 * Creates a new #LassoIdWsf2PsAddToCollectionRequest object.
 *
 * Return value: a newly created #LassoIdWsf2PsAddToCollectionRequest object
 **/
LassoIdWsf2PsAddToCollectionRequest*
lasso_idwsf2_ps_add_to_collection_request_new()
{
	return g_object_new(LASSO_TYPE_IDWSF2_PS_ADD_TO_COLLECTION_REQUEST, NULL);
}