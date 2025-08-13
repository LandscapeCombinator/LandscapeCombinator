#include "LCCommon/Expression.h"
#include "LCCommon/LogLCCommon.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "String/ParseTokens.h"

#define LOCTEXT_NAMESPACE "FLCCommonModule"

FString FExpression::ToString() const
{
    FString Str;

    switch (ExprType)
    {
    case EExprType::Leaf:
        Str = Symbol;
        break;

    case EExprType::Concat:
        Str = TEXT("[");
        for (int32 i = 0; i < Children.Num(); ++i)
        {
            if (!Children[i]) continue;
            Str += Children[i]->ToString();
            if (i < Children.Num() - 1) Str += TEXT(",");
        }
        Str += TEXT("]");
        break;

    case EExprType::Repeat:
        Str = RepeatedChild ? RepeatedChild->ToString() : "?";
        if (RepeatCount > 0)
            Str += FString::Printf(TEXT(" %d"), RepeatCount);
        else
            Str += (Repetition == ERepetitionType::ZeroOrMore) ? TEXT("*") :
                    (Repetition == ERepetitionType::OneOrMore) ? TEXT("+") : TEXT("");
        break;

    case EExprType::RandomChoice:
        Str = TEXT("{");
        for (int32 i = 0; i < Choices.Num(); ++i)
        {
            if (!Choices[i].Child) continue;

            Str += Choices[i].Child->ToString() + FString::Printf(TEXT(":%.2f"), Choices[i].Weight);
            if (i < Choices.Num() - 1) Str += TEXT(",");
        }
        Str += TEXT("}");
        break;
    }

    return Str;
}

FExpression* FExpression::Clone() const
{
    FExpression *Copy = new FExpression(*this);

    Copy->Children.Reset();
    for (auto& Child : Children)
        if (Child) Copy->Children.Add(Child->Clone());

    Copy->Choices.Reset();
    for (auto& Choice : Choices)
        if (Choice.Child) Copy->Choices.Add({ Choice.Child->Clone(), Choice.Weight });

    Copy->RepeatedChild = RepeatedChild ? RepeatedChild->Clone() : nullptr;
    return Copy;
}

TArray<FString> FExpression::Lexer(const FString& Input)
{
    TArray<FString> Tokens;
    FString Current;
    for (TCHAR C : Input)
    {
        if (FChar::IsAlnum(C)) Current.AppendChar(C);
        else
        {
            if (!Current.IsEmpty()) { Tokens.Add(Current); Current.Empty(); }
            if (!FChar::IsWhitespace(C)) Tokens.Add(FString(1, &C));
        }
    }
    if (!Current.IsEmpty()) Tokens.Add(Current);

    return Tokens;
}

FExpression* FExpression::Parse(const TArray<FString>& Tokens, int32& Index)
{
    if (Index >= Tokens.Num())
        return nullptr;

    FString Token = Tokens[Index];
    FExpression* Expr = new FExpression();

    // Leaf
    if (!Token.StartsWith("{") && !Token.StartsWith("["))
    {
        Expr->ExprType = EExprType::Leaf;
        Expr->Symbol = Token;
        Index++;
    }
    // Concat [ ... ]
    else if (Token == "[")
    {
        Expr->ExprType = EExprType::Concat;
        Index++; // skip '['

        while (Index < Tokens.Num() && Tokens[Index] != "]")
        {
            if (Tokens[Index] == ",")
            {
                Index++;
                continue;
            }

            FExpression* Child = Parse(Tokens, Index);
            if (!Child)
            {
                delete Expr;
                return nullptr;
            }
            Expr->Children.Add(Child);
        }

        if (Index >= Tokens.Num() || Tokens[Index] != "]")
        {
            delete Expr;
            return nullptr;
        }
        Index++; // skip ']'
    }
    // RandomChoice { ... }
    else if (Token == "{")
    {
        Expr->ExprType = EExprType::RandomChoice;
        Index++; // skip '{'

        while (Index < Tokens.Num() && Tokens[Index] != "}")
        {
            if (Tokens[Index] == ",")
            {
                Index++;
                continue;
            }

            FExpression* Child = Parse(Tokens, Index);
            if (!Child)
            {
                delete Expr;
                return nullptr;
            }

            float Weight = 1.0f;
            if (Index < Tokens.Num() && Tokens[Index] == ":")
            {
                Index++; // skip ':'
                if (Index >= Tokens.Num())
                {
                    delete Expr;
                    return nullptr;
                }
                Weight = FCString::Atof(*Tokens[Index]);
                Index++;
            }

            Expr->Choices.Add({ Child, Weight });
        }

        if (Index >= Tokens.Num() || Tokens[Index] != "}")
        {
            delete Expr;
            return nullptr;
        }
        Index++; // skip '}'
    }

    // Repetition * / + / count
    if (Index < Tokens.Num())
    {
        FString RepToken = Tokens[Index];
        int32 Count = FCString::Atoi(*RepToken);

        if (RepToken == "*" || RepToken == "+" || Count > 0)
        {
            FExpression* RepeatExpr = new FExpression();
            RepeatExpr->ExprType = EExprType::Repeat;
            RepeatExpr->RepeatedChild = Expr;
            RepeatExpr->Repetition = (RepToken == "*") ? ERepetitionType::ZeroOrMore : ERepetitionType::OneOrMore;
            RepeatExpr->RepeatCount = Count;
            Index++;
            return RepeatExpr;
        }
    }

    return Expr;
}

FExpression* FExpression::Parse(const TArray<FString>& Tokens)
{
    if (Tokens.IsEmpty())
    {
        FExpression *Result = new FExpression();
        Result->ExprType = EExprType::Concat;
        return Result;
    }

    TArray<FString> TokensCopy = Tokens;
    if (TokensCopy[0] != "[" || TokensCopy.Last() != "]")
    {
        TokensCopy.Insert("[", 0);
        TokensCopy.Add("]");
    }

    int32 Index = 0;
    FExpression* ParsedExpression = Parse(TokensCopy, Index);
    if (Index == TokensCopy.Num()) return ParsedExpression;
    else
    {
        UE_LOG(LogLCCommon, Error, TEXT("Internal error while parsing expression"));
        delete ParsedExpression;
        return nullptr;
    }
}

FExpression* FExpression::Parse(const FString& Input)
{
    auto Tokens = Lexer(Input);
    return Parse(Tokens);
}

void FExpression::NormalizeLoops()
{
    if (RepeatedChild) RepeatedChild->NormalizeLoops();
    for (auto& Child : Children) if (Child) Child->NormalizeLoops();
    for (auto& Choice : Choices) if (Choice.Child) Choice.Child->NormalizeLoops();

    if (ExprType == EExprType::Repeat && RepeatedChild)
    {
        if (RepeatCount > 0) // Fixed-count repeat
        {
            for (int i = 0; i < RepeatCount; ++i)
                Children.Add(RepeatedChild->Clone());

            ExprType = EExprType::Concat;
            RepeatCount = 0;

            delete RepeatedChild;
            RepeatedChild = nullptr;
        }
        else if (Repetition == ERepetitionType::OneOrMore) // Convert OneOrMore -> Concat + ZeroOrMore
        {
            FExpression* First = RepeatedChild->Clone();
            FExpression* Rest = new FExpression();
            Rest->ExprType = EExprType::Repeat;
            Rest->RepeatedChild = RepeatedChild; // reuse original pointer
            Rest->Repetition = ERepetitionType::ZeroOrMore;
            Rest->RepeatCount = 0;

            ExprType = EExprType::Concat;
            Children.Empty();
            Children.Add(First);
            Children.Add(Rest);
            RepeatedChild = nullptr;
        }
    }
}

void FExpression::EnqueueTopLevelRepeats(TQueue<FExpression*>& Queue)
{
    if (ExprType == EExprType::Repeat)
    {
        Queue.Enqueue(this);
        return;
    }

    for (FExpression* Child: Children) if (Child) Child->EnqueueTopLevelRepeats(Queue);
}

void FExpression::UnrollOnce()
{
    if (ExprType != EExprType::Repeat || !RepeatedChild) return;

    FExpression* Copy = Clone();

    // Current node becomes Concat
    ExprType = EExprType::Concat;
    Children.Empty();
    Children.Add(RepeatedChild); // repeat child is left of concat
    Children.Add(Copy); // the copy is the right of concat

    RepeatedChild = nullptr;
}

void FExpression::ComputeMinCost(TFunction<double(const FString&)> CostFn)
{
	double Result = 0.0;
	switch (ExprType)
	{
	case EExprType::Leaf:
    {
        MinCost = CostFn(Symbol);
        break;
    }
    case EExprType::Concat:
    {
        MinCost = 0;
        for (auto& Child: Children)
        {
            if (!Child) continue;
            
            Child->ComputeMinCost(CostFn);
            MinCost += Child->MinCost;
        }
		break;
    }
    case EExprType::Repeat:
    {
        if (!RepeatedChild) MinCost = DBL_MAX;
        else
        {
            RepeatedChild->ComputeMinCost(CostFn);

            if (RepeatCount > 0) MinCost = RepeatCount * RepeatedChild->MinCost;
            else if (Repetition == ERepetitionType::ZeroOrMore) MinCost = 0;
            else MinCost = RepeatedChild->MinCost; // OneOrMore
        }
		break;
    }
	case EExprType::RandomChoice:
	{
		MinCost = DBL_MAX;
		for (auto& Choice : Choices)
        {
            if (!Choice.Child) continue;

            Choice.Child->ComputeMinCost(CostFn);
            MinCost = FMath::Min(MinCost, Choice.Child->MinCost);
        }
		break;
	}}
}

void FExpression::MakeChoices()
{
    // Don't go under Repeat nodes
    if (ExprType == EExprType::Repeat) return;

    // Resolve RandomChoice
    if (ExprType == EExprType::RandomChoice && !Choices.IsEmpty())
    {
        int Index = ULCBlueprintLibrary::GetRandomIndex(Choices);
        FExpression* Chosen = Choices[Index].Child;

        if (Chosen) Chosen->MakeChoices();

        for (int i = 0; i < Choices.Num(); i++)
            if (i != Index) delete Choices[i].Child;

        Choices.Empty();

        ExprType      = Chosen->ExprType;
        Symbol        = MoveTemp(Chosen->Symbol);
        Children      = MoveTemp(Chosen->Children);
        RepeatedChild = Chosen->RepeatedChild;
        Repetition    = Chosen->Repetition;
        RepeatCount   = Chosen->RepeatCount;

        Chosen->Children.Empty();
        Chosen->RepeatedChild = nullptr;
        delete Chosen;
    }

    for (FExpression* Child : Children) if (Child) Child->MakeChoices();

    if (RepeatedChild) RepeatedChild->MakeChoices();
}

bool FExpression::Expand(double Size, FString &ExpressionStr, TFunction<double(const FString&)> CostFn, TArray<FString> &OutExpandedVars)
{
    OutExpandedVars.Empty();

    FExpression *Root = Parse(ExpressionStr);
    if (!Root)
    {
        LCReporter::ShowError(FText::Format(
            LOCTEXT("InvalidExpression", "Could not parse expression: {0}.\nPlease check the logs for more details."),
            FText::FromString(ExpressionStr)
        ));
        return false;
    }

    Root->NormalizeLoops();
    Root->MakeChoices();
    Root->ComputeMinCost(CostFn);

    double RemainingSize = Size - Root->MinCost;

    if (RemainingSize >= 0)
    {
        TQueue<FExpression*> Queue;
        Root->EnqueueTopLevelRepeats(Queue);

        while (!Queue.IsEmpty())
        {
            FExpression* Current = nullptr;
            Queue.Dequeue(Current);

            if (!Current || !Current->RepeatedChild) continue;
            if (Current->RepeatedChild->MinCost <= 0 || Current->RepeatedChild->MinCost > RemainingSize) continue;
            
            RemainingSize -= Current->RepeatedChild->MinCost;
            Current->UnrollOnce();
            Current->MakeChoices();
            Current->EnqueueTopLevelRepeats(Queue);
        }
    }

    Root->Flatten(OutExpandedVars);
    delete Root;

    return true;
}

void FExpression::Flatten(TArray<FString>& OutSymbols) const
{
    switch (ExprType)
    {
        case EExprType::Leaf:
            if (!Symbol.IsEmpty()) OutSymbols.Add(Symbol);
            break;

        case EExprType::Concat:
            for (const FExpression* Child : Children)
                if (Child) Child->Flatten(OutSymbols);
            break;

        default:
            break;
    }
}

TArray<FString> FExpression::Flatten() const
{
    TArray<FString> Result;
    Flatten(Result);
    return Result;
}

#undef LOCTEXT_NAMESPACE
