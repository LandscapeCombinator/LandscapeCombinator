// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ERepetitionType : uint8
{
    ZeroOrMore,   // *
    OneOrMore     // +
};

enum class EExprType : uint8
{
    Leaf,
    Concat, // [ Expr1, Expr2, Expr3, ... ]
    Repeat, // Expr* or Expr+ or Expr RepeatCount
    RandomChoice // { Expr1: Weight1, Expr2: Weight2, Expr3: Weight3, ... }
};


struct FExpression;

// Expr:Weight
struct FWeightedChild
{
    FExpression *Child;
    float Weight = 1.0f;
};

struct LCCOMMON_API FExpression
{
    ~FExpression()
    {
        delete RepeatedChild;

        for (auto &Child: Children) delete Child;
        for (auto &Choice: Choices) delete Choice.Child;
    }

    EExprType ExprType = EExprType::Leaf;

    FString Symbol; // used for Leaf
    TArray<FExpression*> Children; // used for Concat
    FExpression* RepeatedChild; // used for Repeat
    ERepetitionType Repetition = ERepetitionType::OneOrMore; // used for Repeat
    int32 RepeatCount = 0; // used for repeat, and overrides 'Repetition' if > 0
    TArray<FWeightedChild> Choices; // used for RandomChoice
    double MinCost = 0;

    FExpression* Clone() const;
    FString ToString() const;

    void ComputeMinCost(TFunction<double(const FString&)> CostFn);
    void NormalizeLoops();
    void MakeChoices();
    void EnqueueTopLevelRepeats(TQueue<FExpression*>& Queue);
    void UnrollOnce();
    void Flatten(TArray<FString>& OutSymbols) const;
    TArray<FString> Flatten() const;
    
    static TArray<FString> Lexer(const FString& Input);
    static FExpression* Parse(const TArray<FString>& Tokens, int32& Index);
    static FExpression* Parse(const TArray<FString>& Tokens);
    static FExpression* Parse(const FString& Input);

    static bool Expand(double Size, FString &ExpressionStr, TFunction<double(const FString&)> Cost, TArray<FString> &OutExpandedVars);
};
